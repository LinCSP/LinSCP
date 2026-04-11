#pragma once
#include <QString>
#include <functional>

namespace linscp::core::ssh { class SshSession; }

namespace linscp::core::scp {

/// SCP fallback для серверов без SFTP.
/// Реализован через ssh_scp_new / ssh_scp_read / ssh_scp_write (libssh).
class ScpClient {
public:
    /// Прогресс: (переданоБайт, всегоБайт)
    using ProgressCallback = std::function<void(qint64 transferred, qint64 total)>;

    /// Скачать один файл (remote → local)
    bool download(ssh::SshSession *session,
                  const QString &remotePath,
                  const QString &localPath,
                  ProgressCallback progress = {});

    /// Загрузить один файл (local → remote)
    bool upload(ssh::SshSession *session,
                const QString &localPath,
                const QString &remotePath,
                ProgressCallback progress = {});

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

} // namespace linscp::core::scp
