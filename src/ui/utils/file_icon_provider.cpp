#include "file_icon_provider.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

namespace linscp {

// All svgIcon() calls happen here — in the GUI thread at construction time.
FileIconProvider::FileIconProvider()
    : m_folder   (svgFolderIcon())
    , m_file     (svgIcon(QStringLiteral("file")))
    , m_fileZip  (svgIcon(QStringLiteral("file-zipper")))
    , m_filePdf  (svgIcon(QStringLiteral("file-pdf")))
    , m_fileImage(svgIcon(QStringLiteral("file-image")))
    , m_fileAudio(svgIcon(QStringLiteral("file-audio")))
    , m_fileVideo(svgIcon(QStringLiteral("file-video")))
    , m_fileCode (svgIcon(QStringLiteral("file-code")))
    , m_fileText (svgIcon(QStringLiteral("file-lines")))
    , m_link     (svgIcon(QStringLiteral("link")))
{}

const QIcon &FileIconProvider::iconForMimeType(const QString &name) const
{
    if (name == QLatin1String("application/zip")              ||
        name == QLatin1String("application/x-tar")            ||
        name == QLatin1String("application/x-bzip2")          ||
        name == QLatin1String("application/x-xz")             ||
        name == QLatin1String("application/x-7z-compressed")  ||
        name == QLatin1String("application/gzip")             ||
        name == QLatin1String("application/zstd")             ||
        name.startsWith(QLatin1String("application/vnd.debian")) ||
        name.startsWith(QLatin1String("application/vnd.rpm")))
        return m_fileZip;

    if (name == QLatin1String("application/pdf"))
        return m_filePdf;

    if (name.startsWith(QLatin1String("image/")))
        return m_fileImage;

    if (name.startsWith(QLatin1String("audio/")))
        return m_fileAudio;

    if (name.startsWith(QLatin1String("video/")))
        return m_fileVideo;

    if (name == QLatin1String("text/x-python")         ||
        name == QLatin1String("text/x-csrc")           ||
        name == QLatin1String("text/x-c++src")         ||
        name == QLatin1String("text/x-chdr")           ||
        name == QLatin1String("text/x-c++hdr")         ||
        name == QLatin1String("text/x-java")           ||
        name == QLatin1String("text/x-script.python")  ||
        name == QLatin1String("text/javascript")       ||
        name == QLatin1String("application/javascript")||
        name == QLatin1String("application/json")      ||
        name == QLatin1String("application/xml")       ||
        name == QLatin1String("text/html")             ||
        name == QLatin1String("text/css")              ||
        name == QLatin1String("application/x-shellscript") ||
        name == QLatin1String("text/x-shellscript"))
        return m_fileCode;

    if (name.startsWith(QLatin1String("text/")))
        return m_fileText;

    return m_file;
}

QIcon FileIconProvider::icon(const QFileInfo &info) const
{
    if (info.isDir())     return m_folder;
    if (info.isSymLink()) return m_link;

    static QMimeDatabase mimeDb;
    const QMimeType mime = mimeDb.mimeTypeForFile(info, QMimeDatabase::MatchExtension);
    return iconForMimeType(mime.name());
}

QIcon FileIconProvider::icon(IconType type) const
{
    return (type == Folder) ? m_folder : m_file;
}

// Free function for use by RemoteFsModel (called from GUI thread only).
QIcon iconForMime(const QMimeType &mime)
{
    // Reuse a temporary provider — safe because we're in the GUI thread.
    static FileIconProvider s_provider;
    return s_provider.iconForMimeType(mime.name());
}

} // namespace linscp
