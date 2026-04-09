#include "checksum.h"
#include "core/ssh/ssh_session.h"
#include <QFile>
#include <QCryptographicHash>

namespace linscp::utils {

QByteArray Checksum::ofLocalFile(const QString &path, ChecksumAlgo algo)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};

    QCryptographicHash hash(algo == ChecksumAlgo::MD5
                                ? QCryptographicHash::Md5
                                : QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result().toHex();
}

QByteArray Checksum::ofRemoteFile(const QString &remotePath, ChecksumAlgo algo,
                                   core::ssh::SshSession *session)
{
    const QString cmd = (algo == ChecksumAlgo::MD5)
                            ? QStringLiteral("md5sum '%1'").arg(remotePath)
                            : QStringLiteral("sha256sum '%1'").arg(remotePath);

    const QString output = session->execCommand(cmd);
    // sha256sum выводит: "<hash>  <filename>"
    const QStringList parts = output.split(' ', Qt::SkipEmptyParts);
    return parts.isEmpty() ? QByteArray{} : parts[0].toUtf8();
}

bool Checksum::equal(const QString &localPath, const QString &remotePath,
                     ChecksumAlgo algo, core::ssh::SshSession *session)
{
    const QByteArray local  = ofLocalFile(localPath, algo);
    const QByteArray remote = ofRemoteFile(remotePath, algo, session);
    return !local.isEmpty() && local == remote;
}

} // namespace linscp::utils
