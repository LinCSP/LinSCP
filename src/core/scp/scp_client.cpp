#include "scp_client.h"
#include "core/ssh/ssh_session.h"
#include <QFile>
#include <QFileInfo>
#include <libssh/libssh.h>

namespace linscp::core::scp {

static constexpr int kBufSize = 32768;

bool ScpClient::download(ssh::SshSession *session,
                         const QString &remotePath,
                         const QString &localPath,
                         ProgressCallback progress)
{
    ssh_scp scp = ssh_scp_new(session->handle(), SSH_SCP_READ,
                              remotePath.toUtf8().constData());
    if (!scp) {
        m_lastError = "Cannot create SCP session";
        return false;
    }
    if (ssh_scp_init(scp) != SSH_OK) {
        m_lastError = QString::fromUtf8(ssh_get_error(session->handle()));
        ssh_scp_free(scp);
        return false;
    }

    const int req = ssh_scp_pull_request(scp);
    if (req != SSH_SCP_REQUEST_NEWFILE) {
        m_lastError = QString::fromUtf8(ssh_get_error(session->handle()));
        ssh_scp_free(scp);
        return false;
    }

    const qint64 fileSize = static_cast<qint64>(ssh_scp_request_get_size(scp));

    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly)) {
        m_lastError = QString("Cannot create local file: %1").arg(localPath);
        ssh_scp_deny_request(scp, "cannot open local file");
        ssh_scp_free(scp);
        return false;
    }

    ssh_scp_accept_request(scp);

    QByteArray buf(kBufSize, 0);
    qint64 received = 0;
    bool ok = true;
    while (received < fileSize) {
        const qint64 remaining = fileSize - received;
        const int toRead = static_cast<int>(qMin(remaining, static_cast<qint64>(kBufSize)));
        const int n = ssh_scp_read(scp, buf.data(), static_cast<size_t>(toRead));
        if (n < 0) { ok = false; break; }
        local.write(buf.constData(), n);
        received += n;
        if (progress) progress(received, fileSize);
    }

    ssh_scp_free(scp);
    if (!ok)
        m_lastError = QString::fromUtf8(ssh_get_error(session->handle()));
    return ok;
}

bool ScpClient::upload(ssh::SshSession *session,
                       const QString &localPath,
                       const QString &remotePath,
                       ProgressCallback progress)
{
    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) {
        m_lastError = QString("Cannot open local file: %1").arg(localPath);
        return false;
    }

    const QFileInfo fi(localPath);
    // remotePath — директория назначения
    ssh_scp scp = ssh_scp_new(session->handle(), SSH_SCP_WRITE,
                              QFileInfo(remotePath).path().toUtf8().constData());
    if (!scp) {
        m_lastError = "Cannot create SCP session";
        return false;
    }
    if (ssh_scp_init(scp) != SSH_OK) {
        m_lastError = QString::fromUtf8(ssh_get_error(session->handle()));
        ssh_scp_free(scp);
        return false;
    }

    if (ssh_scp_push_file(scp, fi.fileName().toUtf8().constData(),
                          static_cast<size_t>(fi.size()), 0644) != SSH_OK) {
        m_lastError = QString::fromUtf8(ssh_get_error(session->handle()));
        ssh_scp_free(scp);
        return false;
    }

    QByteArray buf(kBufSize, 0);
    qint64 sent = 0;
    const qint64 total = fi.size();
    bool ok = true;
    qint64 nread;
    while ((nread = local.read(buf.data(), kBufSize)) > 0) {
        if (ssh_scp_write(scp, buf.constData(), static_cast<size_t>(nread)) != SSH_OK) {
            ok = false; break;
        }
        sent += nread;
        if (progress) progress(sent, total);
    }

    ssh_scp_free(scp);
    if (!ok)
        m_lastError = QString::fromUtf8(ssh_get_error(session->handle()));
    return ok;
}

} // namespace linscp::core::scp
