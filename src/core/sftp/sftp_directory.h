#pragma once
#include "sftp_file.h"
#include <QList>
#include <QString>

namespace linscp::core::sftp {

/// Результат листинга директории
struct SftpDirectory {
    QString           path;
    QList<SftpFileInfo> entries;
    bool              hasError    = false; ///< true если листинг не удался
    QString           errorMessage;        ///< сообщение об ошибке (при hasError)

    /// Только файлы (без директорий)
    QList<SftpFileInfo> files() const;
    /// Только директории
    QList<SftpFileInfo> dirs() const;
};

} // namespace linscp::core::sftp
