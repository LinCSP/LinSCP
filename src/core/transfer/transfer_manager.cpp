#include "transfer_manager.h"
#include "core/sftp/sftp_file.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>

namespace linscp::core::transfer {

TransferManager::TransferManager(IRemoteFileSystem *fs, TransferQueue *queue,
                                 QObject *parent)
    : QObject(parent), m_fs(fs), m_queue(queue)
{
    // Когда в очередь добавляется элемент — будим воркер
    connect(queue, &TransferQueue::itemAdded,
            this,  [this]() { m_sem.release(); },
            Qt::DirectConnection);
}

TransferManager::~TransferManager()
{
    stop();
}

void TransferManager::start()
{
    if (m_thread) return;
    m_stop = false;
    m_thread = QThread::create([this]() { workerLoop(); });
    m_thread->start();
}

void TransferManager::stop()
{
    m_stop = true;
    m_sem.release();          // разбудить воркер, если ждёт следующего задания
    if (m_thread) {
        m_thread->wait(5000);
        delete m_thread;
        m_thread = nullptr;
    }
}

// ── Основной цикл (один фоновый поток) ───────────────────────────────────────

void TransferManager::workerLoop()
{
    while (!m_stop.load()) {
        m_sem.acquire();               // ждём, пока не появится элемент или stop()
        if (m_stop.load()) break;

        auto next = m_queue->nextQueued();
        if (!next) continue;           // семафор мог выйти от stop()

        m_queue->setStatus(next->id, TransferStatus::InProgress);
        const TransferItem item = *next;

        QMetaObject::invokeMethod(this, [this, id = item.id]() {
            emit transferStarted(id);
        }, Qt::QueuedConnection);

        runItem(item);
    }
}

// ── Разрешение конфликтов ─────────────────────────────────────────────────────

OverwritePolicy TransferManager::checkConflict(const TransferItem &item)
{
    const auto global = static_cast<OverwritePolicy>(m_globalPolicy.load());
    if (global == OverwritePolicy::Overwrite) return OverwritePolicy::Overwrite;
    if (global == OverwritePolicy::Skip)      return OverwritePolicy::Skip;

    if (!m_overwriteCb) return OverwritePolicy::Overwrite;

    // Один воркер — нет параллельных операций ФС, stat безопасен из этого потока.
    if (item.direction == TransferDirection::Upload) {
        const sftp::SftpFileInfo remote = m_fs->stat(item.remotePath);
        if (remote.path.isEmpty()) return OverwritePolicy::Overwrite;

        const QFileInfo localFi(item.localPath);
        ConflictInfo src{ item.localPath,  localFi.size(), localFi.lastModified() };
        ConflictInfo dst{ item.remotePath, remote.size,    remote.mtime           };

        OverwritePolicy result = OverwritePolicy::Cancel;
        QMetaObject::invokeMethod(this, [&]() {
            result = m_overwriteCb(src, dst);
        }, Qt::BlockingQueuedConnection);
        return result;

    } else {
        const QFileInfo localFi(item.localPath);
        if (!localFi.exists()) return OverwritePolicy::Overwrite;

        const sftp::SftpFileInfo remote = m_fs->stat(item.remotePath);
        ConflictInfo src{ item.remotePath, remote.size,    remote.mtime     };
        ConflictInfo dst{ item.localPath,  localFi.size(), localFi.lastModified() };

        OverwritePolicy result = OverwritePolicy::Cancel;
        QMetaObject::invokeMethod(this, [&]() {
            result = m_overwriteCb(src, dst);
        }, Qt::BlockingQueuedConnection);
        return result;
    }
}

// ── Исполнение задания ────────────────────────────────────────────────────────

void TransferManager::runItem(const TransferItem &item)
{
    const OverwritePolicy policy = checkConflict(item);

    if (policy == OverwritePolicy::Skip) {
        m_queue->setStatus(item.id, TransferStatus::Cancelled);
        QMetaObject::invokeMethod(this, [this, id = item.id]() {
            emit transferFinished(id, false);
        }, Qt::QueuedConnection);
        return;
    }

    if (policy == OverwritePolicy::Cancel) {
        m_queue->setStatus(item.id, TransferStatus::Cancelled);
        for (const auto &it : m_queue->items())
            if (it.status == TransferStatus::Queued)
                m_queue->setStatus(it.id, TransferStatus::Cancelled);
        QMetaObject::invokeMethod(this, [this, id = item.id]() {
            emit transferFinished(id, false);
        }, Qt::QueuedConnection);
        return;
    }

    // ── Передача ─────────────────────────────────────────────────────────────
    // cumBase накапливает байты завершённых файлов; при старте нового файла
    // p.transferred < prevFileBytes (сброс счётчика), добавляем предыдущее.
    auto progress = [this, &item,
                     prevFileBytes = qint64{0},
                     cumBase       = qint64{0},
                     prevBytes     = qint64{0},
                     chunkTimer    = QElapsedTimer()](
                        const sftp::TransferProgress &p) mutable -> bool
    {
        // Отмена — немедленно останавливаем передачу
        if (m_queue->item(item.id).status == TransferStatus::Cancelled) return false;

        while (m_paused.load() && !m_stop.load()) {
            if (m_queue->item(item.id).status == TransferStatus::Cancelled) return false;
            QThread::msleep(50);
        }
        if (m_stop.load()) return false;
        m_queue->setStatus(item.id, TransferStatus::InProgress);

        // Новый файл начался — prevFileBytes обнулился
        if (p.transferred < prevFileBytes)
            cumBase += prevFileBytes;
        prevFileBytes = p.transferred;

        const int kbps = m_throttleKBps.load();
        if (kbps > 0 && prevBytes > 0) {
            const qint64 chunk    = (cumBase + p.transferred) - prevBytes;
            const qint64 elapsed  = chunkTimer.elapsed();
            const qint64 targetMs = chunk * 1000 / (static_cast<qint64>(kbps) * 1024);
            if (elapsed < targetMs && targetMs - elapsed < 10000)
                QThread::msleep(static_cast<unsigned long>(targetMs - elapsed));
        }
        prevBytes = cumBase + p.transferred;
        chunkTimer.restart();

        m_queue->updateProgress(item.id, cumBase + p.transferred);
        return true;
    };

    // Накапливаем total size на лету при получении листингов (FileZilla-подход)
    qint64 discoveredTotal = 0;
    auto onSizeDiscovered = [this, &item, &discoveredTotal](qint64 size) {
        discoveredTotal += size;
        m_queue->setTotalBytes(item.id, discoveredTotal);
    };

    bool ok = false;
    if (item.direction == TransferDirection::Download) {
        ok = m_fs->downloadRecursive(item.remotePath, item.localPath,
                                     progress, onSizeDiscovered);
    } else if (item.resumeOffset > 0) {
        ok = m_fs->uploadResume(item.localPath, item.remotePath,
                                item.resumeOffset, progress);
    } else {
        ok = m_fs->uploadRecursive(item.localPath, item.remotePath, progress);
    }

    const bool cancelled = (m_queue->item(item.id).status == TransferStatus::Cancelled);
    const QString err = (ok || cancelled) ? QString{} : m_fs->lastError();
    m_queue->setStatus(item.id,
                       ok        ? TransferStatus::Completed :
                       cancelled ? TransferStatus::Cancelled :
                                   TransferStatus::Failed,
                       err);

    QMetaObject::invokeMethod(this, [this, id = item.id, ok]() {
        // Считаем ВСЕ незавершённые задания: Queued + InProgress.
        // Только Queued недостаточно — воркер мог уже поставить следующий элемент
        // в InProgress до того, как этот invokeMethod обработается в UI-потоке.
        // Если считать только Queued, overallProgress(0,0) срабатывает преждевременно
        // (пока ещё идёт передача), вызывает лишний refresh() и из-за
        // конфликта m_generation панель остаётся пустой.
        int remaining = 0;
        for (const auto &it : m_queue->items())
            if (it.status == TransferStatus::Queued ||
                it.status == TransferStatus::InProgress)
                ++remaining;
        emit transferFinished(id, ok);
        emit overallProgress(0, remaining);
    }, Qt::QueuedConnection);
}

} // namespace linscp::core::transfer
