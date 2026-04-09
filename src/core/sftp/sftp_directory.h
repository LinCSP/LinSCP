#pragma once
#include "sftp_file.h"
#include <QList>
#include <QString>

namespace linscp::core::sftp {

/// Результат листинга директории
struct SftpDirectory {
    QString           path;
    QList<SftpFileInfo> entries;

    /// Только файлы (без директорий)
    QList<SftpFileInfo> files() const;
    /// Только директории
    QList<SftpFileInfo> dirs() const;
};

} // namespace linscp::core::sftp
