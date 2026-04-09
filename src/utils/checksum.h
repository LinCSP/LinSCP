#pragma once
#include <QString>
#include <QByteArray>

namespace linscp::core::ssh { class SshSession; }

namespace linscp::utils {

enum class ChecksumAlgo { MD5, SHA256 };

class Checksum {
public:
    /// Вычислить контрольную сумму локального файла
    static QByteArray ofLocalFile(const QString &path, ChecksumAlgo algo = ChecksumAlgo::SHA256);

    /// Вычислить контрольную сумму удалённого файла через ssh exec
    /// (выполняется через SshSession::execCommand)
    static QByteArray ofRemoteFile(const QString &remotePath, ChecksumAlgo algo,
                                   linscp::core::ssh::SshSession *session);

    /// Сравнить файлы по контрольным суммам
    static bool equal(const QString &localPath, const QString &remotePath,
                      ChecksumAlgo algo, core::ssh::SshSession *session);
};

} // namespace linscp::utils
