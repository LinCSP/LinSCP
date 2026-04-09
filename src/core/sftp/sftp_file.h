#pragma once
#include <QString>
#include <QDateTime>
#include <QFileInfo>

namespace linscp::core::sftp {

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
