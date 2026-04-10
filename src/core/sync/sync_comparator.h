#pragma once
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include "sync_profile.h"

// Forward declarations — избегаем циклических зависимостей
namespace linscp::core::sftp { class SftpClient; }
namespace linscp::core::ssh  { class SshSession; }

namespace linscp::core::sync {

enum class SyncAction {
    Upload,     ///< local → remote
    Download,   ///< remote → local
    DeleteRemote,
    DeleteLocal,
    Conflict,   ///< оба изменились
    Skip,
};

struct SyncDiffEntry {
    QString    localPath;
    QString    remotePath;
    SyncAction action = SyncAction::Skip;

    QDateTime  localMtime;
    QDateTime  remoteMtime;
    qint64     localSize  = 0;
    qint64     remoteSize = 0;

    bool isConflict() const { return action == SyncAction::Conflict; }
};

/// Сравнивает локальный и удалённый каталоги, строит список изменений.
/// Вызывать из фонового потока.
class SyncComparator : public QObject {
    Q_OBJECT
public:
    explicit SyncComparator(QObject *parent = nullptr);

    /// Основной метод: вернуть список diff без применения изменений (dry-run).
    /// @param session  опционально — нужен для режима SyncCompareMode::Checksum
    QList<SyncDiffEntry> compare(const SyncProfile &profile,
                                 linscp::core::sftp::SftpClient *sftp,
                                 linscp::core::ssh::SshSession  *session = nullptr);

signals:
    /// Прогресс сканирования (0..100)
    void scanProgress(int percent);

private:
    void scanLocal(const QString &baseLocal, const QString &relPath,
                   const SyncProfile &profile,
                   QHash<QString, QDateTime> &localMap,
                   QHash<QString, qint64>    &localSizes);

    bool matchesExclude(const QString &name, const SyncProfile &profile) const;
};

} // namespace linscp::core::sync
