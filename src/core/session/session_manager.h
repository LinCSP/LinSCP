#pragma once
#include <QObject>
#include <QUuid>
#include <memory>
#include <map>

#include "session_profile.h"
#include "session_store.h"
#include "core/ssh/ssh_session.h"

namespace linscp::core::session {

/// Управляет активными SSH-соединениями.
/// Один SessionManager на всё приложение.
class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(SessionStore *store, QObject *parent = nullptr);

    /// Открыть новое подключение по профилю
    ssh::SshSession *open(const QUuid &profileId);

    /// Получить активную сессию (nullptr если нет)
    ssh::SshSession *active(const QUuid &profileId) const;

    /// Закрыть подключение
    void close(const QUuid &profileId);

    /// Закрыть все
    void closeAll();

    QList<QUuid> activeSessions() const;

signals:
    void sessionOpened(const QUuid &profileId);
    void sessionClosed(const QUuid &profileId);
    void sessionError(const QUuid &profileId, const QString &message);

private:
    SessionStore                                 *m_store;
    std::map<QUuid, std::unique_ptr<ssh::SshSession>> m_sessions;
};

} // namespace linscp::core::session
