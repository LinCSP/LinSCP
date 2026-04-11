#pragma once
#include <QObject>
#include <QThreadPool>
#include <memory>
#include <functional>
#include <atomic>

#include "transfer_queue.h"
#include "transfer_item.h"
#include "core/sftp/sftp_client.h"

namespace linscp::core::transfer {

/// Исполняет задания из TransferQueue в фоновых потоках.
/// Один TransferManager — одна SFTP-сессия.
class TransferManager : public QObject {
    Q_OBJECT
public:
    /// Callback для разрешения конфликта: вызывается из воркер-потока.
    /// Реализация должна при необходимости маршаллить вызов в UI-поток
    /// (напр. через Qt::BlockingQueuedConnection).
    using OverwriteCallback =
        std::function<OverwritePolicy(const ConflictInfo &src,
                                      const ConflictInfo &dst)>;

    explicit TransferManager(sftp::SftpClient *sftp, TransferQueue *queue,
                             QObject *parent = nullptr);

    /// Максимум параллельных передач (по умолчанию 3)
    void setMaxConcurrent(int n);
    int  maxConcurrent() const { return m_pool.maxThreadCount(); }

    /// Установить callback для разрешения конфликтов при перезаписи.
    /// Если не установлен — файлы перезаписываются без вопросов.
    void setOverwriteCallback(OverwriteCallback cb) { m_overwriteCb = std::move(cb); }

    void start();   ///< начать обработку очереди
    void stop();    ///< остановить (текущие задания завершат текущий chunk)

    /// Ограничение скорости, КБ/с. 0 — без ограничений (по умолчанию)
    void setThrottleKBps(int kbps) { m_throttleKBps.store(kbps); }
    int  throttleKBps() const      { return m_throttleKBps.load(); }

    /// Поставить все активные передачи на паузу / снять с паузы
    void pause()  { m_paused.store(true);  }
    void resume() { m_paused.store(false); }

signals:
    void transferStarted(const QUuid &id);
    void transferFinished(const QUuid &id, bool success);
    void overallProgress(int activeCount, int queuedCount);

private slots:
    void onItemAdded(const QUuid &id);
    void scheduleNext();

private:
    void runItem(const TransferItem &item);

    /// Проверить конфликт и вернуть политику.
    /// Если конфликта нет — возвращает OverwritePolicy::Overwrite (продолжать).
    OverwritePolicy checkConflict(const TransferItem &item);

    sftp::SftpClient *m_sftp;
    TransferQueue    *m_queue;
    QThreadPool       m_pool;
    std::atomic<int>  m_active{0};
    bool              m_running = false;

    OverwriteCallback             m_overwriteCb;
    std::atomic<int>              m_globalPolicy{
        static_cast<int>(OverwritePolicy::Ask)}; ///< глобальная политика (OverwriteAll/SkipAll)
    std::atomic<int>              m_throttleKBps{0};   ///< 0 = без ограничений
    std::atomic<bool>             m_paused{false};     ///< пауза всех передач
};

} // namespace linscp::core::transfer
