#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <memory>
#include <libssh/libssh.h>

#include "ssh_auth.h"
#include "ssh_channel.h"
#include "known_hosts.h"

namespace linscp::core::ssh {

enum class SessionState {
    Disconnected,
    Connecting,
    VerifyingHost,  ///< ждём подтверждения fingerprint от пользователя
    Authenticating,
    Connected,
    Error,
};

/// Центральный класс сессии. Один экземпляр на одно подключение.
/// Все SFTP-операции и терминальные вкладки мультиплексируются
/// поверх одной ssh_session_t через отдельные ssh_channel_t.
class SshSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(SessionState state READ state NOTIFY stateChanged)

public:
    explicit SshSession(QObject *parent = nullptr);
    ~SshSession() override;

    // ── Подключение ──────────────────────────────────────────────────────────

    /// Начать асинхронное подключение. Сигналы уведомят о прогрессе.
    void connectToHost(const QString &host, quint16 port,
                       const QString &username, const AuthParams &auth);

    void disconnect();

    SessionState state() const { return m_state; }
    QString lastError() const  { return m_lastError; }

    QString host() const     { return m_host; }
    QString username() const { return m_username; }
    quint16 port() const     { return m_port; }

    // ── Верификация хоста ─────────────────────────────────────────────────────

    /// Fingerprint текущего сервера (доступен после сигнала hostVerificationRequired)
    QByteArray serverFingerprint() const { return m_fingerprint; }

    /// Вызвать после того, как пользователь подтвердил fingerprint (сохраняет в known_hosts)
    void acceptHost();
    /// Принять один раз без сохранения в known_hosts
    void acceptHostOnce();
    /// Вызвать если пользователь отклонил
    void rejectHost();

    // ── Каналы ───────────────────────────────────────────────────────────────

    /// Открыть новый канал (для SFTP-клиента или терминала).
    /// Возвращает nullptr при ошибке.
    SshChannel *openChannel(ChannelType type);

    /// Выполнить одну команду, вернуть stdout (синхронно, для утилит)
    QString execCommand(const QString &cmd);

    ssh_session handle() const { return m_session; }

signals:
    void stateChanged(SessionState state);

    /// Сервер не известен или fingerprint изменился — UI должен спросить пользователя
    void hostVerificationRequired(HostVerifyResult result, const QByteArray &fingerprint);

    void connected();
    void disconnected();
    void errorOccurred(const QString &message);

    /// Прогресс рукопожатия (0..100)
    void connectProgress(int percent);

private slots:
    void doConnect();

private:
    void setState(SessionState s);
    bool authenticate();

    ssh_session  m_session  = nullptr;
    SessionState m_state    = SessionState::Disconnected;
    QString      m_lastError;

    QString      m_host;
    QString      m_username;
    quint16      m_port     = 22;
    AuthParams   m_auth;
    QByteArray   m_fingerprint;

    std::unique_ptr<KnownHosts> m_knownHosts;
};

} // namespace linscp::core::ssh
