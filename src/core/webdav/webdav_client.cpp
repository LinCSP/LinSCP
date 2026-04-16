#include "webdav_client.h"

#ifdef WITH_WEBDAV
#include <neon/ne_session.h>
#include <neon/ne_request.h>
#include <neon/ne_auth.h>
#include <neon/ne_ssl.h>
#include <neon/ne_uri.h>
#include <neon/ne_utils.h>
#include <neon/ne_dates.h>
#include <neon/ne_basic.h>

#include <QXmlStreamReader>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace linscp::core::webdav {

// ─────────────────────────────────────────────────────────────────────────────
//  C-колбэки libneon (только с WITH_WEBDAV)
// ─────────────────────────────────────────────────────────────────────────────

#ifdef WITH_WEBDAV

/// Аутентификация: Basic или Digest (libneon сам выбирает)
static int authCallback(void *userdata, const char *realm, int attempt,
                        char *username, char *password)
{
    Q_UNUSED(realm);
    if (attempt > 0) return 1; // один шанс — иначе ошибка
    const auto *client = static_cast<const WebDavClient *>(userdata);
    qstrncpy(username, client->username().toUtf8().constData(), NE_ABUFSIZ);
    qstrncpy(password, client->password().toUtf8().constData(), NE_ABUFSIZ);
    return 0;
}

/// Верификация SSL-сертификата.
/// TODO: показывать FingerprintDialog как для SSH-хостов.
static int sslVerify(void */*userdata*/, int /*failures*/,
                     const ne_ssl_certificate */*cert*/)
{
    return 0; // принимать любой сертификат
}

// ── Сбор тела ответа в QByteArray ────────────────────────────────────────────
struct BodyBuf { QByteArray data; };

static int bodyReader(void *userdata, const char *buf, size_t len)
{
    static_cast<BodyBuf *>(userdata)->data.append(buf, static_cast<qsizetype>(len));
    return 0;
}

static int acceptAny(void */*ud*/, ne_request */*req*/, const ne_status *st)
{
    return st->klass == 2 || st->code == 207;
}

// ── Скачивание с прогрессом ───────────────────────────────────────────────────
struct DownloadCtx {
    int    fd;
    qint64 downloaded = 0;
    qint64 total      = 0;
    const sftp::ProgressCallback *cb;
};

static int downloadReader(void *userdata, const char *buf, size_t len)
{
    auto *ctx = static_cast<DownloadCtx *>(userdata);
    if (::write(ctx->fd, buf, len) < 0) return -1;
    ctx->downloaded += static_cast<qint64>(len);
    if (ctx->cb && *ctx->cb) {
        sftp::TransferProgress p;
        p.transferred = ctx->downloaded;
        p.total       = ctx->total;
        (*ctx->cb)(p);
    }
    return 0;
}

static int acceptDownload(void */*ud*/, ne_request */*req*/, const ne_status *st)
{
    return st->klass == 2;
}

// ── Загрузка с прогрессом ─────────────────────────────────────────────────────
struct UploadCtx {
    int    fd;
    qint64 total      = 0;
    qint64 uploaded   = 0;
    const sftp::ProgressCallback *cb;
};

static ssize_t uploadProvider(void *userdata, char *buffer, size_t buflen)
{
    auto *ctx = static_cast<UploadCtx *>(userdata);
    const ssize_t n = ::read(ctx->fd, buffer, buflen);
    if (n > 0) {
        ctx->uploaded += static_cast<qint64>(n);
        if (ctx->cb && *ctx->cb) {
            sftp::TransferProgress p;
            p.transferred = ctx->uploaded;
            p.total       = ctx->total;
            (*ctx->cb)(p);
        }
    }
    return n;
}

#endif // WITH_WEBDAV

// ─────────────────────────────────────────────────────────────────────────────

WebDavClient::WebDavClient(WebDavEncryption enc,
                           const QString &host, quint16 port,
                           const QString &username, const QString &password,
                           QObject *parent)
    : QObject(parent)
    , m_username(username)
    , m_password(password)
{
#ifdef WITH_WEBDAV
    const char *scheme = (enc == WebDavEncryption::None) ? "http" : "https";
    m_session = ne_session_create(scheme,
                                  host.toUtf8().constData(),
                                  static_cast<unsigned int>(port));
    if (!m_session) {
        m_lastError = tr("Failed to create neon session");
        return;
    }

    ne_set_server_auth(m_session, authCallback, this);
    ne_set_useragent(m_session, "LinSCP/" LINSCP_VERSION " (libneon)");

    if (enc != WebDavEncryption::None) {
        ne_ssl_trust_default_ca(m_session);
        ne_ssl_set_verify(m_session, sslVerify, this);
    }

    m_connected = true;
#else
    Q_UNUSED(enc); Q_UNUSED(host); Q_UNUSED(port);
    m_lastError = tr("WebDAV support not compiled (rebuild with -DWITH_WEBDAV=ON)");
#endif
}

WebDavClient::~WebDavClient()
{
#ifdef WITH_WEBDAV
    if (m_session) {
        ne_session_destroy(m_session);
        m_session = nullptr;
    }
#endif
}

// ── PROPFIND ──────────────────────────────────────────────────────────────────

QList<WebDavFileInfo> WebDavClient::propfind(const QString &path, int depth)
{
#ifndef WITH_WEBDAV
    Q_UNUSED(path); Q_UNUSED(depth);
    m_lastError = tr("WebDAV not compiled");
    return {};
#else
    if (!m_session) return {};

    static const char kBody[] =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<D:propfind xmlns:D=\"DAV:\">"
          "<D:prop>"
            "<D:displayname/>"
            "<D:getcontentlength/>"
            "<D:getlastmodified/>"
            "<D:resourcetype/>"
            "<D:quota-available-bytes/>"
          "</D:prop>"
        "</D:propfind>";

    const QByteArray pathUtf8 = path.toUtf8();
    ne_request *req = ne_request_create(m_session, "PROPFIND", pathUtf8.constData());

    const QString depthStr = (depth == 0) ? QStringLiteral("0") : QStringLiteral("1");
    ne_add_request_header(req, "Depth", depthStr.toUtf8().constData());
    ne_add_request_header(req, "Content-Type", "application/xml; charset=utf-8");
    ne_set_request_body_buffer(req, kBody, sizeof(kBody) - 1);

    BodyBuf buf;
    ne_add_response_body_reader(req, acceptAny, bodyReader, &buf);

    const int rc = ne_request_dispatch(req);
    const ne_status *st = ne_get_status(req);
    ne_request_destroy(req);

    if (rc != NE_OK) {
        m_lastError = QString::fromUtf8(ne_get_error(m_session));
        return {};
    }
    if (st->code != 207) {
        m_lastError = tr("PROPFIND returned HTTP %1").arg(st->code);
        return {};
    }

    return parsePropfindResponse(buf.data, path);
#endif
}

#ifdef WITH_WEBDAV
/// Разбор 207 Multi-Status XML-ответа через QXmlStreamReader.
QList<WebDavFileInfo> WebDavClient::parsePropfindResponse(const QByteArray &xml,
                                                           const QString &basePath)
{
    QList<WebDavFileInfo> result;
    QXmlStreamReader reader(xml);

    // Стек имён элементов для отслеживания контекста
    WebDavFileInfo current;
    bool inResponse  = false;
    bool inProp      = false;
    bool inPropstat  = false;
    bool isOkStatus  = false;
    QString lastElem;

    while (!reader.atEnd()) {
        reader.readNext();
        const auto ns = reader.namespaceUri();
        const bool isDav = (ns == QLatin1String("DAV:"));

        if (reader.isStartElement()) {
            const QString name = reader.name().toString();

            if (isDav && name == QLatin1String("response")) {
                current = {};
                inResponse = true;
                isOkStatus = false;
            } else if (isDav && name == QLatin1String("propstat")) {
                inPropstat = true;
            } else if (isDav && name == QLatin1String("prop") && inPropstat) {
                inProp = true;
            } else if (isDav && name == QLatin1String("collection") && inProp) {
                current.isCollection = true;
            }
            lastElem = name;

        } else if (reader.isEndElement()) {
            const QString name = reader.name().toString();

            if (isDav && name == QLatin1String("response") && inResponse) {
                if (!current.href.isEmpty())
                    result.append(current);
                inResponse = false;
            } else if (isDav && name == QLatin1String("propstat")) {
                inPropstat = false;
                inProp     = false;
            } else if (isDav && name == QLatin1String("prop")) {
                inProp = false;
            }

        } else if (reader.isCharacters() && !reader.isWhitespace()) {
            const QString text = reader.text().toString();

            if (isDav || lastElem.isEmpty()) {
                // Не пропускаем — нужно проверять через стек; пропускаем вложенные тэги
            }

            // Без учёта namespace — просто смотрим на имя текущего родителя
            // QXmlStreamReader хранит контекст через элементы
            const QXmlStreamNamespaceDeclarations &decls = reader.namespaceDeclarations();
            Q_UNUSED(decls);

            // Читаем по имени родительского элемента
            if (!inResponse) continue;

            // Смотрим на имя текущего элемента: QXmlStreamReader на Characters
            // не имеет прямого доступа к "current element name" — нужно
            // отслеживать через переменную lastElem.
            if (lastElem == QLatin1String("href")) {
                current.href = decodePath(text.toUtf8().constData());
            } else if (inProp && lastElem == QLatin1String("displayname")) {
                current.displayName = text;
            } else if (inProp && lastElem == QLatin1String("getcontentlength")) {
                bool ok = false;
                const qint64 v = text.toLongLong(&ok);
                if (ok) current.contentLength = v;
            } else if (inProp && lastElem == QLatin1String("getlastmodified")) {
                // RFC 1123: "Thu, 01 Jan 2024 00:00:00 GMT"
                const time_t t = ne_rfc1123_parse(text.toLatin1().constData());
                if (t != static_cast<time_t>(-1))
                    current.lastModified = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t),
                                                                          QTimeZone::UTC);
            } else if (inProp && lastElem == QLatin1String("quota-available-bytes")) {
                bool ok = false;
                const qint64 v = text.toLongLong(&ok);
                if (ok) current.quotaAvailable = v;
            } else if (inPropstat && lastElem == QLatin1String("status")) {
                isOkStatus = text.contains(QLatin1String("200 OK"));
                Q_UNUSED(isOkStatus);
            }
        }
    }

    Q_UNUSED(basePath);
    return result;
}
#endif

// ── GET (download) ────────────────────────────────────────────────────────────

bool WebDavClient::get(const QString &remotePath, const QString &localPath,
                       qint64 resumeOffset, sftp::ProgressCallback progress)
{
#ifndef WITH_WEBDAV
    Q_UNUSED(remotePath); Q_UNUSED(localPath);
    Q_UNUSED(resumeOffset); Q_UNUSED(progress);
    m_lastError = tr("WebDAV not compiled");
    return false;
#else
    if (!m_session) return false;

    // Открыть выходной файл
    const int flags = (resumeOffset > 0)
                      ? (O_WRONLY | O_CREAT)
                      : (O_WRONLY | O_CREAT | O_TRUNC);
    const int fd = ::open(localPath.toLocal8Bit().constData(), flags, 0644);
    if (fd < 0) {
        m_lastError = tr("Cannot open local file: %1").arg(localPath);
        return false;
    }
    if (resumeOffset > 0)
        ::lseek(fd, static_cast<off_t>(resumeOffset), SEEK_SET);

    const QByteArray pathUtf8 = remotePath.toUtf8();
    ne_request *req = ne_request_create(m_session, "GET", pathUtf8.constData());

    if (resumeOffset > 0) {
        const QByteArray rangeHdr = QStringLiteral("bytes=%1-").arg(resumeOffset).toUtf8();
        ne_add_request_header(req, "Range", rangeHdr.constData());
    }

    // content-length is read after dispatch via ne_get_response_header
    DownloadCtx ctx{ fd, 0, 0, &progress };
    ne_add_response_body_reader(req, acceptDownload, downloadReader, &ctx);

    const int rc = ne_request_dispatch(req);
    const ne_status *st = ne_get_status(req);
    const int code = st->code;

    // Retroactively set total from Content-Length (progress was approximate without it)
    if (const char *cl = ne_get_response_header(req, "content-length")) {
        bool ok = false;
        const qint64 v = QString::fromLatin1(cl).toLongLong(&ok);
        if (ok) ctx.total = v;
    }
    ne_request_destroy(req);
    ::close(fd);

    if (rc != NE_OK || (code != 200 && code != 206)) {
        m_lastError = (rc != NE_OK)
                      ? QString::fromUtf8(ne_get_error(m_session))
                      : tr("GET returned HTTP %1").arg(code);
        return false;
    }
    return true;
#endif
}

// ── PUT (upload) ──────────────────────────────────────────────────────────────

bool WebDavClient::put(const QString &localPath, const QString &remotePath,
                       qint64 resumeOffset, sftp::ProgressCallback progress)
{
#ifndef WITH_WEBDAV
    Q_UNUSED(localPath); Q_UNUSED(remotePath);
    Q_UNUSED(resumeOffset); Q_UNUSED(progress);
    m_lastError = tr("WebDAV not compiled");
    return false;
#else
    if (!m_session) return false;

    const int fd = ::open(localPath.toLocal8Bit().constData(), O_RDONLY);
    if (fd < 0) {
        m_lastError = tr("Cannot open local file: %1").arg(localPath);
        return false;
    }

    struct stat st{};
    ::fstat(fd, &st);
    const qint64 fileSize = static_cast<qint64>(st.st_size);

    if (resumeOffset > 0)
        ::lseek(fd, static_cast<off_t>(resumeOffset), SEEK_SET);

    const qint64 sendSize = fileSize - resumeOffset;

    const QByteArray pathUtf8 = remotePath.toUtf8();
    ne_request *req = ne_request_create(m_session, "PUT", pathUtf8.constData());

    if (resumeOffset > 0) {
        const QByteArray range = QStringLiteral("bytes %1-%2/%3")
                                     .arg(resumeOffset)
                                     .arg(fileSize - 1)
                                     .arg(fileSize)
                                     .toUtf8();
        ne_add_request_header(req, "Content-Range", range.constData());
    }

    UploadCtx ctx{ fd, sendSize, 0, &progress };
    ne_set_request_body_provider(req, static_cast<ne_off_t>(sendSize),
                                 uploadProvider, &ctx);

    const int rc = ne_request_dispatch(req);
    const int code = ne_get_status(req)->code;
    ne_request_destroy(req);
    ::close(fd);

    if (rc != NE_OK || (code != 200 && code != 201 && code != 204)) {
        m_lastError = (rc != NE_OK)
                      ? QString::fromUtf8(ne_get_error(m_session))
                      : tr("PUT returned HTTP %1").arg(code);
        return false;
    }
    return true;
#endif
}

// ── DELETE ────────────────────────────────────────────────────────────────────

bool WebDavClient::del(const QString &remotePath)
{
#ifndef WITH_WEBDAV
    Q_UNUSED(remotePath);
    m_lastError = tr("WebDAV not compiled");
    return false;
#else
    if (!m_session) return false;
    const int rc = ne_delete(m_session, remotePath.toUtf8().constData());
    if (rc != NE_OK) {
        m_lastError = QString::fromUtf8(ne_get_error(m_session));
        return false;
    }
    return true;
#endif
}

// ── MKCOL ─────────────────────────────────────────────────────────────────────

bool WebDavClient::mkcol(const QString &remotePath)
{
#ifndef WITH_WEBDAV
    Q_UNUSED(remotePath);
    m_lastError = tr("WebDAV not compiled");
    return false;
#else
    if (!m_session) return false;
    const int rc = ne_mkcol(m_session, remotePath.toUtf8().constData());
    if (rc != NE_OK) {
        m_lastError = QString::fromUtf8(ne_get_error(m_session));
        return false;
    }
    return true;
#endif
}

// ── MOVE ──────────────────────────────────────────────────────────────────────

bool WebDavClient::move(const QString &from, const QString &to, bool overwrite)
{
#ifndef WITH_WEBDAV
    Q_UNUSED(from); Q_UNUSED(to); Q_UNUSED(overwrite);
    m_lastError = tr("WebDAV not compiled");
    return false;
#else
    if (!m_session) return false;
    const int rc = ne_move(m_session,
                           overwrite ? 1 : 0,
                           from.toUtf8().constData(),
                           to.toUtf8().constData());
    if (rc != NE_OK) {
        m_lastError = QString::fromUtf8(ne_get_error(m_session));
        return false;
    }
    return true;
#endif
}

// ── COPY ──────────────────────────────────────────────────────────────────────

bool WebDavClient::copy(const QString &from, const QString &to, bool overwrite)
{
#ifndef WITH_WEBDAV
    Q_UNUSED(from); Q_UNUSED(to); Q_UNUSED(overwrite);
    m_lastError = tr("WebDAV not compiled");
    return false;
#else
    if (!m_session) return false;
    const int rc = ne_copy(m_session,
                           overwrite ? 1 : 0,
                           NE_DEPTH_INFINITE,
                           from.toUtf8().constData(),
                           to.toUtf8().constData());
    if (rc != NE_OK) {
        m_lastError = QString::fromUtf8(ne_get_error(m_session));
        return false;
    }
    return true;
#endif
}

// ── Утилиты ───────────────────────────────────────────────────────────────────

QString WebDavClient::decodePath(const char *href)
{
    return QUrl::fromPercentEncoding(QByteArray(href));
}

} // namespace linscp::core::webdav
