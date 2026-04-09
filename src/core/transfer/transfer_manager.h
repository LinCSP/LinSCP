#pragma once
#include <QObject>
#include <QThreadPool>
#include <memory>

#include "transfer_queue.h"
#include "core/sftp/sftp_client.h"

namespace linscp::core::transfer {

/// Исполняет задания из TransferQueue в фоновых потоках.
/// Один TransferManager — одна SFTP-сессия.
class TransferManager : public QObject {
    Q_OBJECT
public:
    explicit TransferManager(sftp::SftpClient *sftp, TransferQueue *queue,
                             QObject *parent = nullptr);

    /// Максимум параллельных передач (по умолчанию 3)
    void setMaxConcurrent(int n);
    int  maxConcurrent() const { return m_pool.maxThreadCount(); }

    void start();   ///< начать обработку очереди
    void stop();    ///< остановить (текущие задания завершат текущий chunk)

signals:
    void transferStarted(const QUuid &id);
    void transferFinished(const QUuid &id, bool success);
    void overallProgress(int activeCount, int queuedCount);

private slots:
    void onItemAdded(const QUuid &id);
    void scheduleNext();

private:
    void runItem(const TransferItem &item);

    sftp::SftpClient *m_sftp;
    TransferQueue    *m_queue;
    QThreadPool       m_pool;
    std::atomic<int>  m_active{0};
    bool              m_running = false;
};

} // namespace linscp::core::transfer
