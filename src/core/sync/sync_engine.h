#pragma once
#include <QObject>
#include <QList>
#include <memory>

#include "sync_profile.h"
#include "sync_comparator.h"
#include "core/sftp/sftp_client.h"
#include "core/transfer/transfer_queue.h"

namespace linscp::core::sync {

/// Оркестратор синхронизации: сравнивает директории и
/// помещает нужные операции в TransferQueue.
class SyncEngine : public QObject {
    Q_OBJECT
public:
    explicit SyncEngine(sftp::SftpClient *sftp,
                        transfer::TransferQueue *queue,
                        QObject *parent = nullptr);

    /// Dry-run: вернуть diff без изменений (вызывать из воркера)
    QList<SyncDiffEntry> preview(const SyncProfile &profile);

    /// Применить diff (вызывать из воркера после подтверждения пользователем)
    void apply(const SyncProfile &profile, const QList<SyncDiffEntry> &diff);

signals:
    void previewReady(const QList<SyncDiffEntry> &diff);
    void syncProgress(int percent);
    void syncFinished(bool success, const QString &error);

private:
    sftp::SftpClient         *m_sftp;
    transfer::TransferQueue  *m_queue;
    SyncComparator            m_comparator;
};

} // namespace linscp::core::sync
