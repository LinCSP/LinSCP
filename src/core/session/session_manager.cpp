#include "session_manager.h"

namespace linscp::core::session {

SessionManager::SessionManager(SessionStore *store, QObject *parent)
    : QObject(parent), m_store(store) {}

ssh::SshSession *SessionManager::open(const QUuid &profileId)
{
    auto it = m_sessions.find(profileId);
    if (it != m_sessions.end())
        return it->second.get();

    const SessionProfile profile = m_store->find(profileId);
    if (!profile.isValid()) return nullptr;

    auto session = std::make_unique<ssh::SshSession>(this);

    connect(session.get(), &ssh::SshSession::connected, this, [this, profileId]() {
        emit sessionOpened(profileId);
    });
    connect(session.get(), &ssh::SshSession::disconnected, this, [this, profileId]() {
        emit sessionClosed(profileId);
    });
    connect(session.get(), &ssh::SshSession::errorOccurred,
            this, [this, profileId](const QString &msg) {
        emit sessionError(profileId, msg);
    });

    ssh::AuthParams auth;
    switch (profile.authMethod) {
    case ssh::AuthMethod::Agent:
        auth = ssh::AuthParams::withAgent();
        break;
    case ssh::AuthMethod::PublicKey:
        auth = ssh::AuthParams::withKey(profile.privateKeyPath);
        break;
    default:
        auth = ssh::AuthParams::withPassword({});
        break;
    }

    ssh::SshSession *raw = session.get();
    m_sessions[profileId] = std::move(session);

    raw->connectToHost(profile.host, profile.port, profile.username, auth);
    return raw;
}

ssh::SshSession *SessionManager::active(const QUuid &profileId) const
{
    auto it = m_sessions.find(profileId);
    return it != m_sessions.end() ? it->second.get() : nullptr;
}

void SessionManager::close(const QUuid &profileId)
{
    auto it = m_sessions.find(profileId);
    if (it != m_sessions.end()) {
        it->second->disconnect();
        m_sessions.erase(it);
    }
}

void SessionManager::closeAll()
{
    for (auto &[id, s] : m_sessions) s->disconnect();
    m_sessions.clear();
}

QList<QUuid> SessionManager::activeSessions() const
{
    QList<QUuid> result;
    for (const auto &[id, s] : m_sessions)
        result.append(id);
    return result;
}

} // namespace linscp::core::session
