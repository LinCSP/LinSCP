#pragma once
#include <QObject>
#include <QList>
#include <QUuid>
#include "sync_profile.h"

namespace linscp::core::sync {

/// Персистентное хранилище профилей синхронизации.
/// Сохраняет список SyncProfile в JSON-файл (~/.config/linscp/sync_profiles.json).
class SyncProfileStore : public QObject {
    Q_OBJECT
public:
    explicit SyncProfileStore(const QString &filePath, QObject *parent = nullptr);

    bool load();
    bool save() const;

    QList<SyncProfile> all() const   { return m_profiles; }

    void add(const SyncProfile &profile);
    void update(const SyncProfile &profile);
    void remove(const QUuid &id);

    /// Найти по id, вернуть дефолтный SyncProfile если не найден
    SyncProfile find(const QUuid &id) const;

signals:
    void changed();

private:
    QString            m_filePath;
    QList<SyncProfile> m_profiles;
};

} // namespace linscp::core::sync
