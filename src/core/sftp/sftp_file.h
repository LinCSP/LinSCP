#pragma once
#include <QString>
#include <QDateTime>
#include <QFileInfo>
#include <functional>

namespace linscp::core::sftp {

struct TransferProgress {
    qint64 transferred = 0;
    qint64 total       = 0;
    int    percent() const {
        return total > 0 ? static_cast<int>(transferred * 100 / total) : 0;
    }
};

using ProgressCallback = std::function<void(const TransferProgress &)>;

/// Информация об удалённом файле/директории (аналог QFileInfo для SFTP)
struct SftpFileInfo {
    QString   name;
    QString   path;           ///< полный путь
    qint64    size    = 0;
    QDateTime mtime;
    QDateTime atime;
    bool      isDir     = false;
    bool      isSymLink = false;
    QString   linkTarget;
    uint      permissions = 0; ///< Unix mode (rwxrwxrwx)
    QString   owner;
    QString   group;

    bool isHidden() const { return name.startsWith('.'); }
    QString permissionsString() const;  ///< "drwxr-xr-x"
};

} // namespace linscp::core::sftp
