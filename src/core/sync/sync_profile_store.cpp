#include "sync_profile_store.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace linscp::core::sync {

// ── helpers ───────────────────────────────────────────────────────────────────

static QJsonObject profileToJson(const SyncProfile &p)
{
    QJsonObject o;
    o["id"]              = p.id.toString(QUuid::WithoutBraces);
    o["name"]            = p.name;
    o["localPath"]       = p.localPath;
    o["remotePath"]      = p.remotePath;
    o["direction"]       = static_cast<int>(p.direction);
    o["compareMode"]     = static_cast<int>(p.compareMode);
    o["excludeHidden"]   = p.excludeHidden;
    o["syncPermissions"] = p.syncPermissions;
    o["syncSymlinks"]    = p.syncSymlinks;

    QJsonArray exc;
    for (const auto &pat : p.excludePatterns) exc.append(pat);
    o["excludePatterns"] = exc;
    return o;
}

static SyncProfile profileFromJson(const QJsonObject &o)
{
    SyncProfile p;
    p.id         = QUuid::fromString(o["id"].toString());
    if (p.id.isNull()) p.id = QUuid::createUuid();
    p.name            = o["name"].toString();
    p.localPath       = o["localPath"].toString();
    p.remotePath      = o["remotePath"].toString();
    p.direction       = static_cast<SyncDirection>(o["direction"].toInt());
    p.compareMode     = static_cast<SyncCompareMode>(o["compareMode"].toInt());
    p.excludeHidden   = o["excludeHidden"].toBool();
    p.syncPermissions = o["syncPermissions"].toBool();
    p.syncSymlinks    = o["syncSymlinks"].toBool();

    for (const auto &v : o["excludePatterns"].toArray())
        p.excludePatterns.append(v.toString());
    return p;
}

// ── SyncProfileStore ──────────────────────────────────────────────────────────

SyncProfileStore::SyncProfileStore(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(filePath)
{}

bool SyncProfileStore::load()
{
    QFile f(m_filePath);
    if (!f.exists()) return true; // нет файла — это нормально
    if (!f.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return false;

    m_profiles.clear();
    for (const auto &v : doc.array())
        m_profiles.append(profileFromJson(v.toObject()));
    return true;
}

bool SyncProfileStore::save() const
{
    QJsonArray arr;
    for (const auto &p : m_profiles)
        arr.append(profileToJson(p));

    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(arr).toJson());
    return true;
}

void SyncProfileStore::add(const SyncProfile &profile)
{
    m_profiles.append(profile);
    save();
    emit changed();
}

void SyncProfileStore::update(const SyncProfile &profile)
{
    for (auto &p : m_profiles) {
        if (p.id == profile.id) {
            p = profile;
            save();
            emit changed();
            return;
        }
    }
}

void SyncProfileStore::remove(const QUuid &id)
{
    const int before = m_profiles.size();
    m_profiles.erase(std::remove_if(m_profiles.begin(), m_profiles.end(),
                                    [&](const SyncProfile &p) { return p.id == id; }),
                     m_profiles.end());
    if (m_profiles.size() != before) {
        save();
        emit changed();
    }
}

SyncProfile SyncProfileStore::find(const QUuid &id) const
{
    for (const auto &p : m_profiles)
        if (p.id == id) return p;
    return {};
}

} // namespace linscp::core::sync
