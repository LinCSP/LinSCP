#include "sync_engine.h"
#include "core/transfer/transfer_item.h"
#include <QFile>
#include <QFileInfo>

namespace linscp::core::sync {

SyncEngine::SyncEngine(sftp::SftpClient *sftp, transfer::TransferQueue *queue,
                       QObject *parent)
    : QObject(parent), m_sftp(sftp), m_queue(queue)
{
    connect(&m_comparator, &SyncComparator::scanProgress,
            this, &SyncEngine::syncProgress);
}

QList<SyncDiffEntry> SyncEngine::preview(const SyncProfile &profile)
{
    return m_comparator.compare(profile, m_sftp);
}

void SyncEngine::apply(const SyncProfile &profile, const QList<SyncDiffEntry> &diff)
{
    int total = diff.size();
    int done  = 0;

    for (const auto &entry : diff) {
        using namespace transfer;

        switch (entry.action) {
        case SyncAction::Upload: {
            TransferItem item;
            item.direction  = TransferDirection::Upload;
            item.localPath  = entry.localPath;
            item.remotePath = entry.remotePath;
            item.totalBytes = entry.localSize;
            m_queue->enqueue(item);
            break;
        }
        case SyncAction::Download: {
            TransferItem item;
            item.direction  = TransferDirection::Download;
            item.localPath  = entry.localPath;
            item.remotePath = entry.remotePath;
            item.totalBytes = entry.remoteSize;
            m_queue->enqueue(item);
            break;
        }
        case SyncAction::DeleteRemote:
            m_sftp->remove(entry.remotePath);
            break;
        case SyncAction::DeleteLocal:
            QFile::remove(entry.localPath);
            break;
        case SyncAction::ChmodRemote:
            // Синхронизация прав: применяем локальные права на сервер
            m_sftp->chmod(entry.remotePath, entry.localPermissions);
            break;
        case SyncAction::SymlinkCreate:
            // Создаём локальный симлинк (remote → local при Download/Bidirectional)
            if (!entry.symlinkTarget.isEmpty()) {
                QFile::remove(entry.localPath); // на случай если уже есть
                QFile::link(entry.symlinkTarget, entry.localPath);
            }
            break;
        default:
            break;
        }

        // После передачи применяем chmod если нужно и запись была upload/download
        if (profile.syncPermissions && entry.permissionMismatch &&
            (entry.action == SyncAction::Upload || entry.action == SyncAction::Download)) {
            if (entry.action == SyncAction::Upload)
                m_sftp->chmod(entry.remotePath, entry.localPermissions);
            // для Download: меняем права локального файла
            if (entry.action == SyncAction::Download && !entry.localPath.isEmpty())
                QFile::setPermissions(
                    entry.localPath,
                    static_cast<QFile::Permissions>(entry.remotePermissions));
        }

        ++done;
        emit syncProgress(done * 100 / total);
    }

    emit syncFinished(true, {});
}

} // namespace linscp::core::sync
