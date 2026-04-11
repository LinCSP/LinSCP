#include "sync_comparator.h"
#include "core/sftp/sftp_client.h"
#include "core/ssh/ssh_session.h"
#include "utils/checksum.h"
#include <QDirIterator>
#include <QRegularExpression>
#include <sys/stat.h>

namespace linscp::core::sync {

SyncComparator::SyncComparator(QObject *parent) : QObject(parent) {}

QList<SyncDiffEntry> SyncComparator::compare(const SyncProfile &profile,
                                              sftp::SftpClient *sftp,
                                              ssh::SshSession  *session)
{
    // 1. Сканируем локаль
    QHash<QString, QDateTime> localMtimes;
    QHash<QString, qint64>    localSizes;
    QHash<QString, uint>      localPerms;
    scanLocal(profile.localPath, QString{}, profile, localMtimes, localSizes, localPerms);
    emit scanProgress(40);

    // 2. Сканируем удалённый (рекурсивно через SFTP листинг)
    QHash<QString, sftp::SftpFileInfo> remoteMap;
    QHash<QString, sftp::SftpFileInfo> remoteSymlinks; // rel → symlink entry
    std::function<void(const QString &)> scanRemote = [&](const QString &rPath) {
        const auto dir = sftp->listDirectory(rPath);
        for (const auto &entry : dir.entries) {
            if (entry.name == "." || entry.name == "..") continue;
            if (matchesExclude(entry.name, profile)) continue;
            if (profile.excludeHidden && entry.isHidden()) continue;

            const QString rel = entry.path.mid(profile.remotePath.length() + 1);
            if (entry.isDir) {
                scanRemote(entry.path);
            } else if (entry.isSymLink && profile.syncSymlinks) {
                remoteSymlinks[rel] = entry;
            } else {
                remoteMap[rel] = entry;
            }
        }
    };
    scanRemote(profile.remotePath);
    emit scanProgress(80);

    // 3. Строим diff
    QList<SyncDiffEntry> result;

    for (auto it = localMtimes.begin(); it != localMtimes.end(); ++it) {
        const QString &rel = it.key();
        SyncDiffEntry entry;
        entry.localPath  = profile.localPath  + '/' + rel;
        entry.remotePath = profile.remotePath + '/' + rel;
        entry.localMtime = it.value();
        entry.localSize  = localSizes.value(rel, 0);

        if (remoteMap.contains(rel)) {
            const auto &rem = remoteMap[rel];
            entry.remoteMtime = rem.mtime;
            entry.remoteSize  = rem.size;

            bool localNewer = entry.localMtime > entry.remoteMtime;
            bool sizeMatch  = entry.localSize  == entry.remoteSize;

            if (profile.compareMode == SyncCompareMode::Checksum && session) {
                // Точное сравнение по SHA-256: пропускаем если суммы совпадают
                const bool same = utils::Checksum::equal(
                    entry.localPath, entry.remotePath,
                    utils::ChecksumAlgo::SHA256, session);
                if (same) {
                    entry.action = SyncAction::Skip;
                } else {
                    // Файлы различаются — определяем направление по mtime
                    if (profile.direction == SyncDirection::Bidirectional
                        && !entry.localMtime.isNull()
                        && !entry.remoteMtime.isNull()
                        && entry.localMtime != entry.remoteMtime) {
                        // Оба изменились — конфликт
                        entry.action = SyncAction::Conflict;
                    } else {
                        entry.action = localNewer ? SyncAction::Upload : SyncAction::Download;
                    }
                }
            } else if (profile.compareMode == SyncCompareMode::MtimeAndSize && sizeMatch)
                entry.action = SyncAction::Skip;
            else if (localNewer)
                entry.action = SyncAction::Upload;
            else
                entry.action = SyncAction::Download;
        } else {
            entry.action = (profile.direction == SyncDirection::RemoteToLocal)
                               ? SyncAction::Skip
                               : SyncAction::Upload;
        }

        // Права: если файл совпадает (Skip/Upload уже определён) и syncPermissions включён
        if (profile.syncPermissions && remoteMap.contains(rel)) {
            const uint lp = localPerms.value(rel, 0);
            const uint rp = remoteMap[rel].permissions & 0777u;
            if (lp != 0 && rp != 0 && lp != rp) {
                entry.permissionMismatch  = true;
                entry.localPermissions    = lp;
                entry.remotePermissions   = rp;
                if (entry.action == SyncAction::Skip)
                    entry.action = SyncAction::ChmodRemote; // только chmod
            }
        }

        if (entry.action != SyncAction::Skip)
            result.append(entry);
    }

    // Файлы только на удалённом
    for (auto it = remoteMap.begin(); it != remoteMap.end(); ++it) {
        if (localMtimes.contains(it.key())) continue;
        SyncDiffEntry entry;
        entry.remotePath  = profile.remotePath + '/' + it.key();
        entry.remoteMtime = it.value().mtime;
        entry.remoteSize  = it.value().size;
        entry.action = (profile.direction == SyncDirection::LocalToRemote)
                           ? SyncAction::DeleteRemote
                           : SyncAction::Download;
        result.append(entry);
    }

    // Симлинки только на удалённом → создать локально (SymlinkCreate при Download/Bidirectional)
    if (profile.syncSymlinks) {
        for (auto it = remoteSymlinks.begin(); it != remoteSymlinks.end(); ++it) {
            if (localMtimes.contains(it.key())) continue;
            if (profile.direction == SyncDirection::LocalToRemote) continue;
            SyncDiffEntry entry;
            entry.remotePath   = profile.remotePath + '/' + it.key();
            entry.localPath    = profile.localPath  + '/' + it.key();
            entry.isSymlink    = true;
            entry.symlinkTarget = sftp->readlink(entry.remotePath);
            entry.action       = SyncAction::SymlinkCreate;
            result.append(entry);
        }
    }

    emit scanProgress(100);
    return result;
}

void SyncComparator::scanLocal(const QString &baseLocal, const QString &relPath,
                               const SyncProfile &profile,
                               QHash<QString, QDateTime> &localMap,
                               QHash<QString, qint64>    &localSizes,
                               QHash<QString, uint>      &localPerms)
{
    const QString absPath = relPath.isEmpty() ? baseLocal
                                              : baseLocal + '/' + relPath;
    QDirIterator it(absPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        if (matchesExclude(fi.fileName(), profile)) continue;
        if (profile.excludeHidden && fi.fileName().startsWith('.')) continue;

        const QString rel = relPath.isEmpty() ? fi.fileName()
                                              : relPath + '/' + fi.fileName();
        if (fi.isDir()) {
            scanLocal(baseLocal, rel, profile, localMap, localSizes, localPerms);
        } else {
            localMap[rel]   = fi.lastModified();
            localSizes[rel] = fi.size();
            // Права в Unix-стиле (rwxrwxrwx)
            if (profile.syncPermissions) {
                struct stat st{};
                if (::stat(fi.absoluteFilePath().toLocal8Bit().constData(), &st) == 0)
                    localPerms[rel] = static_cast<uint>(st.st_mode & 0777u);
            }
        }
    }
}

bool SyncComparator::matchesExclude(const QString &name, const SyncProfile &profile) const
{
    for (const auto &pat : profile.excludePatterns) {
        QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pat));
        if (re.match(name).hasMatch()) return true;
    }
    return false;
}

} // namespace linscp::core::sync
