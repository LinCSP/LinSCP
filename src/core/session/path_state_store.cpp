#include "path_state_store.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace linscp::core::session {

PathStateStore::PathStateStore(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return;

    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QJsonObject o = it.value().toObject();
        PathState s;
        s.localPath        = o["localPath"].toString();
        s.remotePath       = o["remotePath"].toString();
        s.remoteSortColumn = o["remoteSortColumn"].toInt(0);
        s.remoteSortOrder  = o["remoteSortOrder"].toInt(0);
        m_data.insert(QUuid::fromString(it.key()), s);
    }
}

PathState PathStateStore::load(const QUuid &sessionId) const
{
    return m_data.value(sessionId);
}

void PathStateStore::save(const QUuid &sessionId, const PathState &state)
{
    m_data.insert(sessionId, state);
    flush();
}

void PathStateStore::flush() const
{
    QJsonObject root;
    for (auto it = m_data.constBegin(); it != m_data.constEnd(); ++it) {
        QJsonObject o;
        o["localPath"]        = it.value().localPath;
        o["remotePath"]       = it.value().remotePath;
        o["remoteSortColumn"] = it.value().remoteSortColumn;
        o["remoteSortOrder"]  = it.value().remoteSortOrder;
        root[it.key().toString()] = o;
    }
    QFile f(m_filePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

} // namespace linscp::core::session
