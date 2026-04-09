#include "transfer_manager.h"

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
    emit overallProgress(m_active.load(), 0 /* TODO */);
}

void TransferManager::runItem(const TransferItem &item)
{
    bool ok = false;
    auto progress = [this, &item](const sftp::TransferProgress &p) {
        m_queue->updateProgress(item.id, p.transferred);
    };

    if (item.direction == TransferDirection::Download) {
        ok = m_sftp->download(item.remotePath, item.localPath, progress);
    } else {
        if (item.resumeOffset > 0)
            ok = m_sftp->uploadResume(item.localPath, item.remotePath,
                                      item.resumeOffset, progress);
        else
            ok = m_sftp->upload(item.localPath, item.remotePath, progress);
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
