#pragma once
#include <QString>
#include <QStringList>
#include <QUuid>

namespace linscp::core::sync {

enum class SyncDirection {
    LocalToRemote,
    RemoteToLocal,
    Bidirectional,
};

enum class SyncCompareMode {
    MtimeAndSize,   ///< быстрый — сравнивать mtime + размер
    Checksum,       ///< точный — MD5/SHA256 через remote exec
};

/// Сохраняемый профиль синхронизации
struct SyncProfile {
    QUuid          id       = QUuid::createUuid();
    QString        name;

    QString        localPath;
    QString        remotePath;

    SyncDirection    direction   = SyncDirection::LocalToRemote;
    SyncCompareMode  compareMode = SyncCompareMode::MtimeAndSize;

    QStringList    excludePatterns;  ///< glob-маски (*.tmp, .git, ...)
    bool           excludeHidden   = false;
    bool           syncPermissions = false;
    bool           syncSymlinks    = false;

    bool isValid() const {
        return !localPath.isEmpty() && !remotePath.isEmpty();
    }
};

} // namespace linscp::core::sync
