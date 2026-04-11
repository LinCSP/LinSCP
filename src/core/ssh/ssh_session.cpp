#include "ssh_session.h"
#include <QDir>
#include <QtConcurrent/QtConcurrent>
#include <libssh/libssh.h>

namespace linscp::core::ssh {

SshSession::SshSession(QObject *parent)
    : QObject(parent)
    , m_knownHosts(std::make_unique<KnownHosts>(
          QDir::homePath() + "/.config/linscp/known_hosts"))
{
    m_session = ssh_new();
}

SshSession::~SshSession()
{
    // Сигнализируем воркеру о завершении и прерываем блокирующий ssh_connect/auth
    m_aborting = true;
    if (m_session)
        ssh_disconnect(m_session);  // прерывает ssh_connect() в воркере

    // Ждём завершения воркер-потока прежде чем освободить m_session
    if (m_workerFuture.isRunning())
        m_workerFuture.waitForFinished();

    if (m_session) {
        ssh_free(m_session);
        m_session = nullptr;
    }
}

void SshSession::connectToHost(const QString &host, quint16 port,
                                const QString &username, const AuthParams &auth)
{
    if (m_state != SessionState::Disconnected) {
        disconnect();
    }

    m_host     = host;
    m_port     = port;
    m_username = username;
    m_auth     = auth;

    // Соединение выполняется в воркер-потоке чтобы не блокировать UI.
    // Сигналы emitятся из воркера — Qt автоматически ставит их в очередь
    // к UI-потоку (AutoConnection → QueuedConnection для cross-thread).
    m_aborting = false;
    setState(SessionState::Connecting);
    m_workerFuture = QtConcurrent::run([this]() { doConnect(); });
}

void SshSession::setProxyJump(const QString &proxyHost, quint16 proxyPort,
                              const QString &proxyUser)
{
    m_proxyJumpHost = proxyHost;
    m_proxyJumpPort = proxyPort;
    m_proxyJumpUser = proxyUser;
}

void SshSession::doConnect()
{
    if (m_aborting) return;

    emit logMessage(tr("Looking for host %1…").arg(m_host));
    ssh_options_set(m_session, SSH_OPTIONS_HOST, m_host.toUtf8().constData());
    ssh_options_set(m_session, SSH_OPTIONS_PORT, &m_port);
    ssh_options_set(m_session, SSH_OPTIONS_USER, m_username.toUtf8().constData());

    // Jump Host (ProxyJump): ssh -W target:port [-p proxyPort] [user@]proxyHost
    if (!m_proxyJumpHost.isEmpty()) {
        const QString userAt = m_proxyJumpUser.isEmpty()
                               ? m_proxyJumpHost
                               : m_proxyJumpUser + "@" + m_proxyJumpHost;
        const QString cmd = QString("ssh -W %1:%2 -p %3 -o StrictHostKeyChecking=accept-new %4")
                                .arg(m_host).arg(m_port).arg(m_proxyJumpPort).arg(userAt);
        const QByteArray cmdBa = cmd.toUtf8();
        ssh_options_set(m_session, SSH_OPTIONS_PROXYCOMMAND, cmdBa.constData());
        emit logMessage(tr("Using jump host %1:%2…").arg(m_proxyJumpHost).arg(m_proxyJumpPort));
    }

    emit connectProgress(10);
    emit logMessage(tr("Connecting to %1:%2…").arg(m_host).arg(m_port));

    if (ssh_connect(m_session) != SSH_OK) {
        if (m_aborting) return;
        m_lastError = QString::fromUtf8(ssh_get_error(m_session));
        setState(SessionState::Error);
        emit errorOccurred(m_lastError);
        return;
    }
    if (m_aborting) return;

    emit connectProgress(40);
    emit logMessage(tr("Connected. Verifying host key…"));

    // Fingerprint хоста
    ssh_key key = nullptr;
    ssh_get_server_publickey(m_session, &key);
    unsigned char *hash = nullptr;
    size_t hashLen = 0;
    ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &hashLen);
    ssh_key_free(key);
    m_fingerprint = QByteArray(reinterpret_cast<char *>(hash),
                               static_cast<qsizetype>(hashLen));
    ssh_clean_pubkey_hash(&hash);

    HostVerifyResult hvr = m_knownHosts->verify(m_host, m_port, m_fingerprint);
    if (hvr != HostVerifyResult::Ok) {
        setState(SessionState::VerifyingHost);
        emit hostVerificationRequired(hvr, m_fingerprint);
        return;
    }

    emit connectProgress(60);
    if (!authenticate()) return;
    if (m_aborting) return;

    emit connectProgress(100);
    emit logMessage(tr("Opening session…"));
    setState(SessionState::Connected);
    emit connected();
}

void SshSession::acceptHost()
{
    m_knownHosts->accept(m_host, m_port, m_fingerprint);
    m_workerFuture = QtConcurrent::run([this]() {
        if (m_aborting) return;
        if (!authenticate()) return;
        if (m_aborting) return;
        emit logMessage(tr("Opening session…"));
        setState(SessionState::Connected);
        emit connected();
    });
}

void SshSession::acceptHostOnce()
{
    m_workerFuture = QtConcurrent::run([this]() {
        if (m_aborting) return;
        if (!authenticate()) return;
        if (m_aborting) return;
        emit logMessage(tr("Opening session…"));
        setState(SessionState::Connected);
        emit connected();
    });
}

void SshSession::rejectHost()
{
    disconnect();
    emit errorOccurred(tr("Host verification rejected by user."));
}

bool SshSession::authenticate()
{
    if (m_aborting) return false;
    setState(SessionState::Authenticating);
    emit logMessage(tr("Authenticating as %1…").arg(m_username));

    int rc = SSH_AUTH_ERROR;
    switch (m_auth.method) {
    case AuthMethod::Password:
        emit logMessage(tr("Using username \"%1\".").arg(m_username));
        emit logMessage(tr("Authenticating with pre-entered password."));
        rc = ssh_userauth_password(m_session, nullptr,
                                   m_auth.password.toUtf8().constData());
        break;

    case AuthMethod::PublicKey: {
        emit logMessage(tr("Authenticating with public key \"%1\".").arg(m_auth.privateKeyPath));
        ssh_key privKey = nullptr;
        const char *pass = m_auth.passphrase.isEmpty()
                               ? nullptr
                               : m_auth.passphrase.toUtf8().constData();
        if (ssh_pki_import_privkey_file(m_auth.privateKeyPath.toUtf8().constData(),
                                        pass, nullptr, nullptr, &privKey) != SSH_OK) {
            m_lastError = tr("Cannot load private key: %1").arg(m_auth.privateKeyPath);
            setState(SessionState::Error);
            emit errorOccurred(m_lastError);
            return false;
        }
        rc = ssh_userauth_publickey(m_session, nullptr, privKey);
        ssh_key_free(privKey);
        break;
    }

    case AuthMethod::Agent:
        emit logMessage(tr("Authenticating via SSH agent."));
        rc = ssh_userauth_agent(m_session, nullptr);
        break;

    case AuthMethod::Interactive:
        rc = SSH_AUTH_ERROR;
        break;
    }

    if (m_aborting) return false;

    if (rc != SSH_AUTH_SUCCESS) {
        m_lastError = QString::fromUtf8(ssh_get_error(m_session));
        setState(SessionState::Error);
        emit errorOccurred(m_lastError);
        return false;
    }
    emit logMessage(tr("Authentication successful."));
    return true;
}

void SshSession::disconnect()
{
    if (m_session && ssh_is_connected(m_session)) {
        ssh_disconnect(m_session);
    }
    setState(SessionState::Disconnected);
    emit disconnected();
}

SshChannel *SshSession::openChannel(ChannelType type)
{
    if (m_state != SessionState::Connected) return nullptr;

    ssh_channel ch = ssh_channel_new(m_session);
    if (!ch) return nullptr;

    if (ssh_channel_open_session(ch) != SSH_OK) {
        ssh_channel_free(ch);
        return nullptr;
    }

    // Пересылка SSH-агента для Shell-каналов (терминал → можно ssh дальше)
    if (type == ChannelType::Shell && m_agentForwarding) {
        if (ssh_channel_request_auth_agent(ch) != SSH_OK)
            emit logMessage(tr("Warning: agent forwarding request failed."));
    }

    return new SshChannel(ch, type, this);
}

QString SshSession::execCommand(const QString &cmd)
{
    SshChannel *ch = openChannel(ChannelType::Exec);
    if (!ch) return {};

    if (ssh_channel_request_exec(ch->handle(), cmd.toUtf8().constData()) != SSH_OK) {
        ch->close();
        ch->deleteLater();
        return {};
    }

    QByteArray output;
    char buf[4096];
    int nbytes;
    while ((nbytes = ssh_channel_read(ch->handle(), buf, sizeof(buf), 0)) > 0) {
        output.append(buf, nbytes);
    }

    ch->close();
    ch->deleteLater();
    return QString::fromUtf8(output).trimmed();
}

void SshSession::setState(SessionState s)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged(s);
    }
}

} // namespace linscp::core::ssh
