#pragma once
#include <QString>
#include <QByteArray>

namespace linscp::core::ssh {

/// Метод аутентификации SSH
enum class AuthMethod {
    Password,
    PublicKey,
    Agent,          ///< ssh-agent (Unix socket)
    Interactive,    ///< keyboard-interactive
};

/// Параметры аутентификации, передаются в SshSession::connect()
struct AuthParams {
    AuthMethod method = AuthMethod::Password;

    // Password / keyboard-interactive
    QString password;

    // PublicKey
    QString privateKeyPath;     ///< путь к файлу ключа
    QString passphrase;         ///< passphrase (пусто — без защиты)

    // Agent
    QString agentSocket;        ///< путь к Unix-сокету агента (по умолч. $SSH_AUTH_SOCK)

    static AuthParams withPassword(const QString &pwd) {
        AuthParams p;
        p.method   = AuthMethod::Password;
        p.password = pwd;
        return p;
    }

    static AuthParams withKey(const QString &keyPath, const QString &pass = {}) {
        AuthParams p;
        p.method         = AuthMethod::PublicKey;
        p.privateKeyPath = keyPath;
        p.passphrase     = pass;
        return p;
    }

    static AuthParams withAgent(const QString &socket = {}) {
        AuthParams p;
        p.method      = AuthMethod::Agent;
        p.agentSocket = socket;
        return p;
    }
};

} // namespace linscp::core::ssh
