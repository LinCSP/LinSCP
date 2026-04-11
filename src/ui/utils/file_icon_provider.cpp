#include "file_icon_provider.h"
#include "svg_icon.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

namespace linscp {

static QMimeDatabase s_mimeDb;

QIcon iconForMime(const QMimeType &mime)
{
    const QString name = mime.name();
    const QString generic = mime.genericIconName(); // e.g. "text-x-generic"

    // Archives
    if (name == QLatin1String("application/zip")         ||
        name == QLatin1String("application/x-tar")       ||
        name == QLatin1String("application/x-bzip2")     ||
        name == QLatin1String("application/x-xz")        ||
        name == QLatin1String("application/x-7z-compressed") ||
        name == QLatin1String("application/gzip")        ||
        name == QLatin1String("application/zstd")        ||
        name.startsWith(QLatin1String("application/vnd.debian")) ||
        name.startsWith(QLatin1String("application/vnd.rpm")))
        return svgIcon(QStringLiteral("file-zipper"));

    // PDF
    if (name == QLatin1String("application/pdf"))
        return svgIcon(QStringLiteral("file-pdf"));

    // Images
    if (name.startsWith(QLatin1String("image/")))
        return svgIcon(QStringLiteral("file-image"));

    // Audio
    if (name.startsWith(QLatin1String("audio/")))
        return svgIcon(QStringLiteral("file-audio"));

    // Video
    if (name.startsWith(QLatin1String("video/")))
        return svgIcon(QStringLiteral("file-video"));

    // Source code / scripts
    if (name == QLatin1String("text/x-python")        ||
        name == QLatin1String("text/x-csrc")          ||
        name == QLatin1String("text/x-c++src")        ||
        name == QLatin1String("text/x-chdr")          ||
        name == QLatin1String("text/x-c++hdr")        ||
        name == QLatin1String("text/x-java")          ||
        name == QLatin1String("text/x-script.python") ||
        name == QLatin1String("text/javascript")      ||
        name == QLatin1String("application/javascript")||
        name == QLatin1String("application/json")     ||
        name == QLatin1String("application/xml")      ||
        name == QLatin1String("text/html")            ||
        name == QLatin1String("text/css")             ||
        name == QLatin1String("application/x-shellscript") ||
        name == QLatin1String("text/x-shellscript")   ||
        generic == QLatin1String("text-x-script"))
        return svgIcon(QStringLiteral("file-code"));

    // Plain text / documents / markdown / yaml / config
    if (name.startsWith(QLatin1String("text/")))
        return svgIcon(QStringLiteral("file-lines"));

    // Fallback — generic file
    return svgIcon(QStringLiteral("file"));
}

// ── FileIconProvider ─────────────────────────────────────────────────────────

QIcon FileIconProvider::icon(const QFileInfo &info) const
{
    if (info.isDir())
        return svgFolderIcon();
    if (info.isSymLink())
        return svgIcon(QStringLiteral("link"));

    const QMimeType mime = s_mimeDb.mimeTypeForFile(info, QMimeDatabase::MatchExtension);
    return iconForMime(mime);
}

QIcon FileIconProvider::icon(IconType type) const
{
    switch (type) {
    case Folder:    return svgFolderIcon();
    case File:      return svgIcon(QStringLiteral("file"));
    default:        return {};
    }
}

} // namespace linscp
