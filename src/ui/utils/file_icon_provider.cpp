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

    // Use extension only — QFileInfo::suffix() is pure string ops, safe from any thread.
    // QMimeDatabase must NOT be used here because QFileSystemModel calls icon()
    // from its FileInfoGatherer background thread, and QMimeDatabase is not thread-safe
    // when a shared instance is accessed concurrently.
    const QString ext = info.suffix().toLower();

    if (ext == QLatin1String("zip")  || ext == QLatin1String("tar")  ||
        ext == QLatin1String("gz")   || ext == QLatin1String("bz2")  ||
        ext == QLatin1String("xz")   || ext == QLatin1String("zst")  ||
        ext == QLatin1String("7z")   || ext == QLatin1String("rar")  ||
        ext == QLatin1String("deb")  || ext == QLatin1String("rpm"))
        return m_fileZip;

    if (ext == QLatin1String("pdf"))
        return m_filePdf;

    if (ext == QLatin1String("jpg")  || ext == QLatin1String("jpeg") ||
        ext == QLatin1String("png")  || ext == QLatin1String("gif")  ||
        ext == QLatin1String("bmp")  || ext == QLatin1String("svg")  ||
        ext == QLatin1String("webp") || ext == QLatin1String("ico")  ||
        ext == QLatin1String("tiff") || ext == QLatin1String("tif"))
        return m_fileImage;

    if (ext == QLatin1String("mp3")  || ext == QLatin1String("wav")  ||
        ext == QLatin1String("ogg")  || ext == QLatin1String("flac") ||
        ext == QLatin1String("aac")  || ext == QLatin1String("m4a")  ||
        ext == QLatin1String("opus"))
        return m_fileAudio;

    if (ext == QLatin1String("mp4")  || ext == QLatin1String("avi")  ||
        ext == QLatin1String("mkv")  || ext == QLatin1String("mov")  ||
        ext == QLatin1String("wmv")  || ext == QLatin1String("webm") ||
        ext == QLatin1String("flv")  || ext == QLatin1String("m4v"))
        return m_fileVideo;

    if (ext == QLatin1String("cpp")  || ext == QLatin1String("cxx")  ||
        ext == QLatin1String("c")    || ext == QLatin1String("h")    ||
        ext == QLatin1String("hpp")  || ext == QLatin1String("hxx")  ||
        ext == QLatin1String("py")   || ext == QLatin1String("js")   ||
        ext == QLatin1String("ts")   || ext == QLatin1String("mjs")  ||
        ext == QLatin1String("html") || ext == QLatin1String("htm")  ||
        ext == QLatin1String("css")  || ext == QLatin1String("json") ||
        ext == QLatin1String("xml")  || ext == QLatin1String("sh")   ||
        ext == QLatin1String("bash") || ext == QLatin1String("java") ||
        ext == QLatin1String("go")   || ext == QLatin1String("rs")   ||
        ext == QLatin1String("php")  || ext == QLatin1String("rb")   ||
        ext == QLatin1String("swift")|| ext == QLatin1String("kt"))
        return m_fileCode;

    if (ext == QLatin1String("txt")  || ext == QLatin1String("md")   ||
        ext == QLatin1String("rst")  || ext == QLatin1String("log")  ||
        ext == QLatin1String("csv")  || ext == QLatin1String("yaml") ||
        ext == QLatin1String("yml")  || ext == QLatin1String("toml") ||
        ext == QLatin1String("ini")  || ext == QLatin1String("conf") ||
        ext == QLatin1String("cfg"))
        return m_fileText;

    return m_file;
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
