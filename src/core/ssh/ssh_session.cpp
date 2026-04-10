#include "ssh_session.h"
#include <QDir>
#include <QThread>
#include <QThreadPool>
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
    disconnect();
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
    setState(SessionState::Connecting);
    QThreadPool::globalInstance()->start([this]() { doConnect(); });
}

void SshSession::doConnect()
{
    ssh_options_set(m_session, SSH_OPTIONS_HOST, m_host.toUtf8().constData());
    ssh_options_set(m_session, SSH_OPTIONS_PORT, &m_port);
    ssh_options_set(m_session, SSH_OPTIONS_USER, m_username.toUtf8().constData());

    emit connectProgress(10);

    if (ssh_connect(m_session) != SSH_OK) {
        m_lastError = QString::fromUtf8(ssh_get_error(m_session));
        setState(SessionState::Error);
        emit errorOccurred(m_lastError);
        return;
    }

    emit connectProgress(40);

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
        // Ждём acceptHost() / rejectHost() от UI
        return;
    }

    emit connectProgress(60);
    if (!authenticate()) return;

    emit connectProgress(100);
    setState(SessionState::Connected);
    emit connected();
}

void SshSession::acceptHost()
{
    m_knownHosts->accept(m_host, m_port, m_fingerprint);
    QThreadPool::globalInstance()->start([this]() {
        if (!authenticate()) return;
        setState(SessionState::Connected);
        emit connected();
    });
}

void SshSession::acceptHostOnce()
{
    QThreadPool::globalInstance()->start([this]() {
        if (!authenticate()) return;
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
    setState(SessionState::Authenticating);

    int rc = SSH_AUTH_ERROR;
    switch (m_auth.method) {
    case AuthMethod::Password:
        rc = ssh_userauth_password(m_session, nullptr,
                                   m_auth.password.toUtf8().constData());
        break;

    case AuthMethod::PublicKey: {
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
        rc = ssh_userauth_agent(m_session, nullptr);
        break;

    case AuthMethod::Interactive:
        // TODO: keyboard-interactive через сигнал в UI
        rc = SSH_AUTH_ERROR;
        break;
    }

    if (rc != SSH_AUTH_SUCCESS) {
        m_lastError = QString::fromUtf8(ssh_get_error(m_session));
        setState(SessionState::Error);
        emit errorOccurred(m_lastError);
        return false;
    }
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
