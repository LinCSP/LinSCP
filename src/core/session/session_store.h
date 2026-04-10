#pragma once
#include <QObject>
#include <QList>
#include <QUuid>
#include "session_profile.h"

namespace linscp::core::session {

/// Персистентное хранилище профилей.
/// Данные шифруются AES-256-GCM (ключ из keyring или мастер-пароля).
class SessionStore : public QObject {
    Q_OBJECT
public:
    explicit SessionStore(const QString &filePath, QObject *parent = nullptr);

    bool load(const QString &masterPassword = {});
    bool save(const QString &masterPassword = {}) const;

    QList<SessionProfile> all() const;
    SessionProfile        find(const QUuid &id) const;

    void add(const SessionProfile &profile);
    void update(const SessionProfile &profile);
    void remove(const QUuid &id);

    // ── Папки ─────────────────────────────────────────────────────────────────
    /// Явно сохранённые пути папок (напр. "A-media", "A-media/Cobalt-pro")
    QStringList folders() const;
    void addFolder(const QString &path);
    void removeFolder(const QString &path);

    /// Экспорт в JSON (без шифрования — для резервной копии)
    bool exportJson(const QString &path) const;
    /// Импорт из JSON
    bool importJson(const QString &path);

signals:
    void changed();

private:
    QString                m_filePath;
    QList<SessionProfile>  m_profiles;
    QStringList            m_folders;   ///< явно созданные папки

    QByteArray encrypt(const QByteArray &data, const QString &password) const;
    QByteArray decrypt(const QByteArray &data, const QString &password) const;
};

} // namespace linscp::core::session
