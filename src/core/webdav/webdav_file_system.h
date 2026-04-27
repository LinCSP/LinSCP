#pragma once
#include "core/i_remote_file_system.h"
#include "webdav_client.h"

namespace linscp::core::session { struct SessionProfile; }

namespace linscp::core::webdav {

/// IRemoteFileSystem поверх WebDavClient.
/// Аналог WinSCP TWebDAVFileSystem — маппит DAV-операции в общий интерфейс.
class WebDavFileSystem : public IRemoteFileSystem {
    Q_OBJECT
public:
    /// Создать из профиля сессии (извлекает host, port, credentials, encryption).
    explicit WebDavFileSystem(const session::SessionProfile &profile,
                              QObject *parent = nullptr);
    explicit WebDavFileSystem(WebDavEncryption enc,
                              const QString &host, quint16 port,
                              const QString &username, const QString &password,
                              QObject *parent = nullptr);

    bool isConnected() const;

    // ── IRemoteFileSystem ─────────────────────────────────────────────────────
    QList<sftp::SftpFileInfo> list(const QString &path) override;
    sftp::SftpFileInfo        stat(const QString &path) override;

    bool download(const QString &remotePath, const QString &localPath,
                  sftp::ProgressCallback progress = {}) override;
    bool upload(const QString &localPath, const QString &remotePath,
                sftp::ProgressCallback progress = {}) override;
    bool uploadResume(const QString &localPath, const QString &remotePath,
                      qint64 offset, sftp::ProgressCallback progress = {}) override;
    bool uploadRecursive(const QString &localPath, const QString &remotePath,
                         sftp::ProgressCallback progress = {}) override;
    bool downloadRecursive(const QString &remotePath, const QString &localPath,
                           sftp::ProgressCallback progress = {},
                           SizeCallback onSizeDiscovered = {}) override;

    bool rename(const QString &oldPath, const QString &newPath) override;
    bool remove(const QString &remotePath) override;
    bool mkdir(const QString &remotePath) override;
    bool removeRecursive(const QString &remotePath) override;

    bool supportsChmod()     const override { return false; }
    bool supportsSymlinks()  const override { return false; }
    bool supportsOwner()     const override { return false; }
    bool supportsFreeSpace() const override { return true;  }

    qint64 freeSpace(const QString &path) override;

    QString lastError() const override;

private:
    /// Преобразовать WebDavFileInfo в SftpFileInfo (для единого интерфейса модели)
    static sftp::SftpFileInfo toSftpInfo(const WebDavFileInfo &dav, const QString &parentPath);

    WebDavClient *m_client;
};

} // namespace linscp::core::webdav
