#include "sftp_client.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <libssh/sftp.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace linscp::core::sftp {

SftpClient::SftpClient(ssh::SshSession *session, QObject *parent)
    : QObject(parent), m_session(session)
{
    init();
}

SftpClient::~SftpClient()
{
    if (m_sftp) {
        sftp_free(m_sftp);
        m_sftp = nullptr;
    }
}

bool SftpClient::init()
{
    m_sftp = sftp_new(m_session->handle());
    if (!m_sftp) {
        m_lastError = tr("Failed to create SFTP session");
        return false;
    }
    if (sftp_init(m_sftp) != SSH_OK) {
        m_lastError = tr("SFTP init error: %1").arg(sftp_get_error(m_sftp));
        sftp_free(m_sftp);
        m_sftp = nullptr;
        return false;
    }
    return true;
}

SftpDirectory SftpClient::listDirectory(const QString &remotePath)
{
    SftpDirectory result;
    result.path = remotePath;

    sftp_dir dir = sftp_opendir(m_sftp, remotePath.toUtf8().constData());
    if (!dir) {
        m_lastError = tr("Cannot open directory: %1").arg(remotePath);
        emit errorOccurred(m_lastError);
        return result;
    }

    sftp_attributes attr;
    while ((attr = sftp_readdir(m_sftp, dir)) != nullptr) {
        const QString name = QString::fromUtf8(attr->name);
        if (name == "." || name == "..") {
            sftp_attributes_free(attr);
            continue;
        }

        SftpFileInfo info;
        info.name        = name;
        // QDir::cleanPath убирает двойные слеши (напр. при remotePath == "/")
        info.path        = QDir::cleanPath(remotePath + '/' + name);
        info.size        = static_cast<qint64>(attr->size);
        info.mtime       = QDateTime::fromSecsSinceEpoch(attr->mtime);
        info.atime       = QDateTime::fromSecsSinceEpoch(attr->atime);
        info.isDir       = (attr->type == SSH_FILEXFER_TYPE_DIRECTORY);
        info.isSymLink   = (attr->type == SSH_FILEXFER_TYPE_SYMLINK);
        info.permissions = attr->permissions;
        if (attr->owner) info.owner = QString::fromUtf8(attr->owner);
        if (attr->group) info.group = QString::fromUtf8(attr->group);

        result.entries.append(info);
        sftp_attributes_free(attr);
    }

    sftp_closedir(dir);
    return result;
}

SftpFileInfo SftpClient::stat(const QString &remotePath)
{
    sftp_attributes attr = sftp_stat(m_sftp, remotePath.toUtf8().constData());
    if (!attr) return {};

    SftpFileInfo info;
    info.name        = QFileInfo(remotePath).fileName();
    info.path        = remotePath;
    info.size        = static_cast<qint64>(attr->size);
    info.mtime       = QDateTime::fromSecsSinceEpoch(attr->mtime);
    info.isDir       = (attr->type == SSH_FILEXFER_TYPE_DIRECTORY);
    info.isSymLink   = (attr->type == SSH_FILEXFER_TYPE_SYMLINK);
    info.permissions = attr->permissions;
    sftp_attributes_free(attr);
    return info;
}

bool SftpClient::download(const QString &remotePath, const QString &localPath,
                          ProgressCallback progress)
{
    sftp_file remote = sftp_open(m_sftp, remotePath.toUtf8().constData(),
                                 O_RDONLY, 0);
    if (!remote) {
        m_lastError = tr("Cannot open remote file: %1").arg(remotePath);
        emit errorOccurred(m_lastError);
        return false;
    }

    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly)) {
        sftp_close(remote);
        m_lastError = tr("Cannot create local file: %1").arg(localPath);
        emit errorOccurred(m_lastError);
        return false;
    }

    const qint64 total = stat(remotePath).size;
    TransferProgress tp;
    tp.total = total;

    char buf[kChunkSize];
    ssize_t nread;
    while ((nread = sftp_read(remote, buf, sizeof(buf))) > 0) {
        local.write(buf, nread);
        tp.transferred += nread;
        if (progress) progress(tp);
    }

    sftp_close(remote);
    return nread == 0; // 0 = EOF, -1 = error
}

bool SftpClient::upload(const QString &localPath, const QString &remotePath,
                        ProgressCallback progress)
{
    return uploadResume(localPath, remotePath, 0, progress);
}

bool SftpClient::uploadResume(const QString &localPath, const QString &remotePath,
                              qint64 offset, ProgressCallback progress)
{
    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) {
        m_lastError = tr("Cannot open local file: %1").arg(localPath);
        emit errorOccurred(m_lastError);
        return false;
    }

    int flags = O_WRONLY | O_CREAT;
    if (offset > 0) flags |= O_APPEND;
    else            flags |= O_TRUNC;

    sftp_file remote = sftp_open(m_sftp, remotePath.toUtf8().constData(),
                                 flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (!remote) {
        m_lastError = tr("Cannot open remote file for writing: %1").arg(remotePath);
        emit errorOccurred(m_lastError);
        return false;
    }

    if (offset > 0) {
        local.seek(offset);
        sftp_seek64(remote, static_cast<uint64_t>(offset));
    }

    TransferProgress tp;
    tp.total       = local.size();
    tp.transferred = offset;

    char buf[kChunkSize];
    qint64 nread;
    while ((nread = local.read(buf, sizeof(buf))) > 0) {
        if (sftp_write(remote, buf, static_cast<size_t>(nread)) < 0) {
            sftp_close(remote);
            return false;
        }
        tp.transferred += nread;
        if (progress) progress(tp);
    }

    sftp_close(remote);
    return true;
}

bool SftpClient::rename(const QString &oldPath, const QString &newPath)
{
    return sftp_rename(m_sftp,
                       oldPath.toUtf8().constData(),
                       newPath.toUtf8().constData()) == SSH_OK;
}

bool SftpClient::remove(const QString &remotePath)
{
    return sftp_unlink(m_sftp, remotePath.toUtf8().constData()) == SSH_OK;
}

bool SftpClient::mkdir(const QString &remotePath)
{
    return sftp_mkdir(m_sftp, remotePath.toUtf8().constData(),
                      S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == SSH_OK;
}

bool SftpClient::rmdir(const QString &remotePath)
{
    return sftp_rmdir(m_sftp, remotePath.toUtf8().constData()) == SSH_OK;
}

bool SftpClient::chmod(const QString &remotePath, uint mode)
{
    return sftp_chmod(m_sftp, remotePath.toUtf8().constData(), mode) == SSH_OK;
}

} // namespace linscp::core::sftp
