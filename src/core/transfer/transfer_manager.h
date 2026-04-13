#pragma once
#include <QObject>
#include <QSemaphore>
#include <functional>
#include <atomic>

#include "transfer_queue.h"
#include "transfer_item.h"
#include "core/sftp/sftp_client.h"

class QThread;

namespace linscp::core::transfer {

/// Исполняет задания из TransferQueue в одном фоновом потоке (последовательно).
/// Один поток — один SFTP-канал, никаких гонок.
class TransferManager : public QObject {
    Q_OBJECT
public:
    /// Callback для разрешения конфликта — вызывается в UI-потоке.
    using OverwriteCallback =
        std::function<OverwritePolicy(const ConflictInfo &src,
                                      const ConflictInfo &dst)>;

    explicit TransferManager(sftp::SftpClient *sftp, TransferQueue *queue,
                             QObject *parent = nullptr);
    ~TransferManager() override;

    void setOverwriteCallback(OverwriteCallback cb) { m_overwriteCb = std::move(cb); }

    /// Установить глобальную политику (OverwriteAll / SkipAll).
    void setGlobalOverwritePolicy(OverwritePolicy p) {
        m_globalPolicy.store(static_cast<int>(p));
    }

    void start();
    void stop();

    void setThrottleKBps(int kbps) { m_throttleKBps.store(kbps); }
    int  throttleKBps() const      { return m_throttleKBps.load(); }

    void pause()  { m_paused.store(true);  }
    void resume() { m_paused.store(false); }

signals:
    void transferStarted(const QUuid &id);
    void transferFinished(const QUuid &id, bool success);
    void overallProgress(int activeCount, int queuedCount);

private:
    void workerLoop();
    void runItem(const TransferItem &item);
    OverwritePolicy checkConflict(const TransferItem &item);

    sftp::SftpClient *m_sftp;
    TransferQueue    *m_queue;

    OverwriteCallback  m_overwriteCb;
    QThread           *m_thread    = nullptr;
    QSemaphore         m_sem;                          ///< сигнал «есть новый элемент»

    std::atomic<int>   m_globalPolicy{ static_cast<int>(OverwritePolicy::Ask) };
    std::atomic<int>   m_throttleKBps{ 0 };
    std::atomic<bool>  m_paused{ false };
    std::atomic<bool>  m_stop{ false };
};

} // namespace linscp::core::transfer
