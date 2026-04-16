#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <functional>

#include "webdav_file_info.h"
#include "core/sftp/sftp_file.h"  // TransferProgress / ProgressCallback

#ifdef WITH_WEBDAV
struct ne_session_s;
typedef struct ne_session_s ne_session;
#endif

namespace linscp::core::webdav {

/// Тип шифрования WebDAV-соединения.
/// Аналог WinSCP TFtpEncryptionMode для WebDAV.
enum class WebDavEncryption {
    None        = 0,   ///< plain HTTP (порт 80)
    Tls         = 1,   ///< HTTPS (порт 443), STARTTLS не требуется
    ImplicitTls = 2,   ///< HTTPS с явным TLS (тот же 443, но клиент не ждёт STARTTLS)
};

/// WebDAV-клиент поверх libneon.
/// Аналог WinSCP TWebDAVFileSystem + NeonIntf.
/// Все методы синхронные — вызывать из воркер-потока.
class WebDavClient : public QObject {
    Q_OBJECT
public:
    explicit WebDavClient(WebDavEncryption enc,
                          const QString &host, quint16 port,
                          const QString &username, const QString &password,
                          QObject *parent = nullptr);
    ~WebDavClient() override;

    bool isConnected() const { return m_connected; }

    // ── DAV-методы ────────────────────────────────────────────────────────────

    /// PROPFIND: листинг директории (depth=1) или stat файла (depth=0).
    QList<WebDavFileInfo> propfind(const QString &path, int depth = 1);

    /// GET: скачать файл; resumeOffset > 0 — добавить Range: bytes=offset-
    bool get(const QString &remotePath, const QString &localPath,
             qint64 resumeOffset = 0,
             sftp::ProgressCallback progress = {});

    /// PUT: загрузить файл; resumeOffset > 0 — добавить Content-Range: bytes=offset-size-1/total
    bool put(const QString &localPath, const QString &remotePath,
             qint64 resumeOffset = 0,
             sftp::ProgressCallback progress = {});

    /// DELETE удалённого ресурса (файл или коллекция рекурсивно)
    bool del(const QString &remotePath);

    /// MKCOL: создать коллекцию
    bool mkcol(const QString &remotePath);

    /// MOVE: переместить / переименовать
    bool move(const QString &from, const QString &to, bool overwrite = true);

    /// COPY: скопировать
    bool copy(const QString &from, const QString &to, bool overwrite = true);

    QString lastError() const { return m_lastError; }

    // Доступ для authCallback (C-колбэка libneon)
    QString username() const { return m_username; }
    QString password() const { return m_password; }

private:
    /// Декодировать URL-encoded href в чистый путь
    static QString decodePath(const char *href);

    /// Разобрать XML-ответ PROPFIND в список WebDavFileInfo
    static QList<WebDavFileInfo> parsePropfindResponse(const QByteArray &xml,
                                                        const QString &basePath);

    QString m_username;
    QString m_password;
    QString m_lastError;
    bool    m_connected = false;

#ifdef WITH_WEBDAV
    ne_session *m_session = nullptr;
#endif
};

} // namespace linscp::core::webdav
