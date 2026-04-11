#include "transfer_manager.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>

namespace linscp::core::transfer {

TransferManager::TransferManager(sftp::SftpClient *sftp, TransferQueue *queue,
                                 QObject *parent)
    : QObject(parent), m_sftp(sftp), m_queue(queue)
{
    m_pool.setMaxThreadCount(3);
    connect(queue, &TransferQueue::itemAdded, this, &TransferManager::onItemAdded);
}

void TransferManager::setMaxConcurrent(int n)
{
    m_pool.setMaxThreadCount(n);
}

void TransferManager::start()
{
    m_running = true;
    scheduleNext();
}

void TransferManager::stop()
{
    m_running = false;
}

void TransferManager::onItemAdded(const QUuid &)
{
    if (m_running) scheduleNext();
}

void TransferManager::scheduleNext()
{
    while (m_running && m_active < m_pool.maxThreadCount()) {
        auto next = m_queue->nextQueued();
        if (!next) break;

        m_queue->setStatus(next->id, TransferStatus::InProgress);
        ++m_active;
        emit transferStarted(next->id);

        const TransferItem item = *next;
        m_pool.start([this, item]() {
            runItem(item);
        });
    }
    int queued = 0;
    for (const auto &it : m_queue->items())
        if (it.status == TransferStatus::Queued) ++queued;
    emit overallProgress(m_active.load(), queued);
}

// ── Разрешение конфликтов ──────────────────────────────────────────────────────

OverwritePolicy TransferManager::checkConflict(const TransferItem &item)
{
    // Глобальная политика (OverwriteAll / SkipAll) — не спрашиваем заново
    const auto global = static_cast<OverwritePolicy>(m_globalPolicy.load());
    if (global == OverwritePolicy::Overwrite) return OverwritePolicy::Overwrite;
    if (global == OverwritePolicy::Skip)      return OverwritePolicy::Skip;

    if (item.direction == TransferDirection::Upload) {
        // Проверяем существует ли файл на сервере
        const sftp::SftpFileInfo remote = m_sftp->stat(item.remotePath);
        if (remote.path.isEmpty()) return OverwritePolicy::Overwrite; // не существует

        // Файл существует → спросить (если есть callback)
        if (!m_overwriteCb) return OverwritePolicy::Overwrite; // нет callback — перезаписать

        const QFileInfo localFi(item.localPath);
        ConflictInfo src{item.localPath, localFi.size(), localFi.lastModified()};
        ConflictInfo dst{item.remotePath, remote.size, remote.mtime};

        return m_overwriteCb(src, dst);

    } else {
        // Download: проверяем существует ли локальный файл
        const QFileInfo localFi(item.localPath);
        if (!localFi.exists()) return OverwritePolicy::Overwrite; // не существует

        if (!m_overwriteCb) return OverwritePolicy::Overwrite;

        const sftp::SftpFileInfo remote = m_sftp->stat(item.remotePath);
        ConflictInfo src{item.remotePath, remote.size, remote.mtime};
        ConflictInfo dst{item.localPath, localFi.size(), localFi.lastModified()};

        return m_overwriteCb(src, dst);
    }
}

// ── Исполнение задания ────────────────────────────────────────────────────────

void TransferManager::runItem(const TransferItem &item)
{
    // ── Проверка конфликта ────────────────────────────────────────────────────
    const OverwritePolicy policy = checkConflict(item);

    if (policy == OverwritePolicy::Skip) {
        m_queue->setStatus(item.id, TransferStatus::Cancelled);
        --m_active;
        QMetaObject::invokeMethod(this, [this, item]() {
            emit transferFinished(item.id, false);
            scheduleNext();
        }, Qt::QueuedConnection);
        return;
    }

    if (policy == OverwritePolicy::Cancel) {
        // Отменить весь оставшийся поток — сбросить всю очередь
        m_queue->setStatus(item.id, TransferStatus::Cancelled);
        // Отменяем все Queued задания
        for (const auto &it : m_queue->items()) {
            if (it.status == TransferStatus::Queued)
                m_queue->setStatus(it.id, TransferStatus::Cancelled);
        }
        --m_active;
        QMetaObject::invokeMethod(this, [this, item]() {
            emit transferFinished(item.id, false);
            scheduleNext();
        }, Qt::QueuedConnection);
        return;
    }

    // ── Передача файла ────────────────────────────────────────────────────────
    bool ok = false;

    // Callback с поддержкой паузы и троттлинга
    auto progress = [this, &item,
                     prevBytes   = qint64{0},
                     chunkTimer  = QElapsedTimer()](const sftp::TransferProgress &p) mutable
    {
        // Пауза: ждём снятия флага, проверяем отмену
        while (m_paused.load() && m_running) {
            if (m_queue->item(item.id).status == TransferStatus::Cancelled) return;
            QThread::msleep(50);
        }
        m_queue->setStatus(item.id, TransferStatus::InProgress);

        // Троттлинг: если чанк прилетел быстрее, чем разрешено — спим
        const int kbps = m_throttleKBps.load();
        if (kbps > 0 && prevBytes > 0) {
            const qint64 chunkBytes = p.transferred - prevBytes;
            const qint64 elapsedMs  = chunkTimer.elapsed();
            const qint64 targetMs   = chunkBytes * 1000 / (static_cast<qint64>(kbps) * 1024);
            if (elapsedMs < targetMs && (targetMs - elapsedMs) < 10000)
                QThread::msleep(static_cast<unsigned long>(targetMs - elapsedMs));
        }
        prevBytes = p.transferred;
        chunkTimer.restart();

        m_queue->updateProgress(item.id, p.transferred);
    };

    if (item.direction == TransferDirection::Download) {
        ok = m_sftp->downloadRecursive(item.remotePath, item.localPath, progress);
    } else {
        if (item.resumeOffset > 0)
            ok = m_sftp->uploadResume(item.localPath, item.remotePath,
                                      item.resumeOffset, progress);
        else
            ok = m_sftp->uploadRecursive(item.localPath, item.remotePath, progress);
    }

    const QString err = ok ? QString{} : m_sftp->lastError();
    m_queue->setStatus(item.id,
                       ok ? TransferStatus::Completed : TransferStatus::Failed,
                       err);
    --m_active;

    QMetaObject::invokeMethod(this, [this, item, ok]() {
        emit transferFinished(item.id, ok);
        scheduleNext();
    }, Qt::QueuedConnection);
}

} // namespace linscp::core::transfer
