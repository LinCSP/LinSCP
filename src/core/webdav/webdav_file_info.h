#pragma once
#include <QString>
#include <QDateTime>

namespace linscp::core::webdav {

/// Результат разбора одного <D:response> в PROPFIND-ответе.
/// Аналог WinSCP TRemoteFile, заполненный из DAV: пространства имён.
struct WebDavFileInfo {
    QString   href;                  ///< декодированный URL-путь
    QString   displayName;           ///< DAV:displayname
    qint64    contentLength = 0;     ///< DAV:getcontentlength
    QDateTime lastModified;          ///< DAV:getlastmodified (RFC 1123)
    bool      isCollection = false;  ///< DAV:resourcetype = <D:collection/>
    qint64    quotaAvailable = -1;   ///< DAV:quota-available-bytes (-1 = нет данных)
};

} // namespace linscp::core::webdav
