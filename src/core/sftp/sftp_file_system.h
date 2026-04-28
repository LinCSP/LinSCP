#pragma once
#include "core/i_remote_file_system.h"
#include "sftp_client.h"

namespace linscp::core::ssh { class SshSession; }

namespace linscp::core::sftp {

/// IRemoteFileSystem-адаптер поверх SftpClient — для SFTP и SCP протоколов.
/// Делегирует все вызовы в SftpClient без изменения логики.
class SftpFileSystem : public IRemoteFileSystem {
    Q_OBJECT
public:
    explicit SftpFileSystem(SftpClient *sftp, QObject *parent = nullptr);

    /// SSH-сессия для df-запроса freeSpace() — вызывать после создания
    void setSshSession(ssh::SshSession *session) { m_sshSession = session; }

    QList<SftpFileInfo> list(const QString &path) override;
    SftpFileInfo        stat(const QString &path) override;

    bool download(const QString &remotePath, const QString &localPath,
                  ProgressCallback progress = {}) override;
    bool upload(const QString &localPath, const QString &remotePath,
                ProgressCallback progress = {}) override;
    bool uploadResume(const QString &localPath, const QString &remotePath,
                      qint64 offset, ProgressCallback progress = {}) override;
    bool uploadRecursive(const QString &localPath, const QString &remotePath,
                         ProgressCallback progress = {},
                         SizeCallback onSizeDiscovered = {}) override;
    bool downloadRecursive(const QString &remotePath, const QString &localPath,
                           ProgressCallback progress = {},
                           SizeCallback onSizeDiscovered = {}) override;
    qint64 calcSizeRecursive(const QString &path) override;

    bool rename(const QString &oldPath, const QString &newPath) override;
    bool remove(const QString &remotePath) override;
    bool mkdir(const QString &remotePath) override;
    bool rmdir(const QString &remotePath) override;
    bool chmod(const QString &remotePath, uint mode) override;
    QString readlink(const QString &remotePath) override;
    bool symlink(const QString &target, const QString &linkPath) override;
    bool removeRecursive(const QString &remotePath) override;

    bool supportsChmod()     const override { return true; }
    bool supportsSymlinks()  const override { return true; }
    bool supportsOwner()     const override { return true; }
    bool supportsFreeSpace() const override { return true; }

    qint64 freeSpace(const QString &path) override;

    QString lastError() const override;

private:
    SftpClient      *m_sftp;
    ssh::SshSession *m_sshSession = nullptr;
};

} // namespace linscp::core::sftp
