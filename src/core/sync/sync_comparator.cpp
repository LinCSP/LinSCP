#include "sync_comparator.h"
#include "core/sftp/sftp_client.h"
#include <QDirIterator>
#include <QRegularExpression>

namespace linscp::core::sync {

SyncComparator::SyncComparator(QObject *parent) : QObject(parent) {}

QList<SyncDiffEntry> SyncComparator::compare(const SyncProfile &profile,
                                              sftp::SftpClient *sftp)
{
    // 1. Сканируем локаль
    QHash<QString, QDateTime> localMtimes;
    QHash<QString, qint64>    localSizes;
    scanLocal(profile.localPath, QString{}, profile, localMtimes, localSizes);
    emit scanProgress(40);

    // 2. Сканируем удалённый (рекурсивно через SFTP листинг)
    QHash<QString, sftp::SftpFileInfo> remoteMap;
    std::function<void(const QString &)> scanRemote = [&](const QString &rPath) {
        const auto dir = sftp->listDirectory(rPath);
        for (const auto &entry : dir.entries) {
            if (entry.name == "." || entry.name == "..") continue;
            if (matchesExclude(entry.name, profile)) continue;
            if (profile.excludeHidden && entry.isHidden()) continue;

            const QString rel = entry.path.mid(profile.remotePath.length() + 1);
            if (entry.isDir)
                scanRemote(entry.path);
            else
                remoteMap[rel] = entry;
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

            if (profile.compareMode == SyncCompareMode::MtimeAndSize && sizeMatch)
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

    emit scanProgress(100);
    return result;
}

void SyncComparator::scanLocal(const QString &baseLocal, const QString &relPath,
                               const SyncProfile &profile,
                               QHash<QString, QDateTime> &localMap,
                               QHash<QString, qint64> &localSizes)
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
        if (fi.isDir())
            scanLocal(baseLocal, rel, profile, localMap, localSizes);
        else {
            localMap[rel]   = fi.lastModified();
            localSizes[rel] = fi.size();
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
