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
        m_data.insert(QUuid::fromString(it.key()),
                      {o["localPath"].toString(), o["remotePath"].toString()});
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
        o["localPath"]  = it.value().localPath;
        o["remotePath"] = it.value().remotePath;
        root[it.key().toString()] = o;
    }
    QFile f(m_filePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

} // namespace linscp::core::session
