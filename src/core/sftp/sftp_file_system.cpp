#include "sftp_file_system.h"
#include "core/ssh/ssh_session.h"

namespace linscp::core::sftp {

SftpFileSystem::SftpFileSystem(SftpClient *sftp, QObject *parent)
    : IRemoteFileSystem(parent), m_sftp(sftp) {}

QList<SftpFileInfo> SftpFileSystem::list(const QString &path)
{
    return m_sftp->listDirectory(path).entries;
}

SftpFileInfo SftpFileSystem::stat(const QString &path)
{
    return m_sftp->stat(path);
}

bool SftpFileSystem::download(const QString &remotePath, const QString &localPath,
                              ProgressCallback progress)
{
    return m_sftp->downloadAsync(remotePath, localPath, std::move(progress));
}

bool SftpFileSystem::upload(const QString &localPath, const QString &remotePath,
                            ProgressCallback progress)
{
    return m_sftp->upload(localPath, remotePath, std::move(progress));
}

bool SftpFileSystem::uploadResume(const QString &localPath, const QString &remotePath,
                                  qint64 offset, ProgressCallback progress)
{
    return m_sftp->uploadResume(localPath, remotePath, offset, std::move(progress));
}

bool SftpFileSystem::uploadRecursive(const QString &localPath, const QString &remotePath,
                                     ProgressCallback progress, SizeCallback onSizeDiscovered)
{
    return m_sftp->uploadRecursive(localPath, remotePath,
                                   std::move(progress), std::move(onSizeDiscovered));
}

bool SftpFileSystem::downloadRecursive(const QString &remotePath, const QString &localPath,
                                       ProgressCallback progress, SizeCallback onSizeDiscovered)
{
    return m_sftp->downloadRecursive(remotePath, localPath,
                                     std::move(progress), std::move(onSizeDiscovered));
}

qint64 SftpFileSystem::calcSizeRecursive(const QString &path)
{
    return m_sftp->calcSizeRecursive(path);
}

bool SftpFileSystem::rename(const QString &oldPath, const QString &newPath)
{
    return m_sftp->rename(oldPath, newPath);
}

bool SftpFileSystem::remove(const QString &remotePath)
{
    return m_sftp->remove(remotePath);
}

bool SftpFileSystem::mkdir(const QString &remotePath)
{
    return m_sftp->mkdir(remotePath);
}

bool SftpFileSystem::rmdir(const QString &remotePath)
{
    return m_sftp->rmdir(remotePath);
}

bool SftpFileSystem::chmod(const QString &remotePath, uint mode)
{
    return m_sftp->chmod(remotePath, mode);
}

QString SftpFileSystem::readlink(const QString &remotePath)
{
    return m_sftp->readlink(remotePath);
}

bool SftpFileSystem::symlink(const QString &target, const QString &linkPath)
{
    return m_sftp->symlink(target, linkPath);
}

bool SftpFileSystem::removeRecursive(const QString &remotePath)
{
    return m_sftp->removeRecursive(remotePath);
}

qint64 SftpFileSystem::freeSpace(const QString &path)
{
    if (!m_sshSession) return -1;
    // df -P <path>: выводит 1024-блочные единицы (POSIX)
    const QString cmd = QStringLiteral("df -P '%1' 2>/dev/null | awk 'NR==2{print $4}'")
                            .arg(QString(path).replace('\'', "'\\''"));
    const QString out = m_sshSession->execCommand(cmd).trimmed();
    if (out.isEmpty()) return -1;
    bool ok = false;
    const qint64 blocks = out.toLongLong(&ok);
    return ok ? blocks * 1024LL : -1;
}

QString SftpFileSystem::lastError() const
{
    return m_sftp->lastError();
}

} // namespace linscp::core::sftp
