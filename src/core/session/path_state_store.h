#pragma once
#include <QHash>
#include <QObject>
#include <QUuid>

namespace linscp::core::session {

struct PathState {
    QString localPath;
    QString remotePath;
};

/// Хранит последние открытые пути (local + remote) для каждой сессии.
/// Сохраняется в ~/.config/linscp/path_state.json (без шифрования).
class PathStateStore : public QObject {
    Q_OBJECT
public:
    explicit PathStateStore(const QString &filePath, QObject *parent = nullptr);

    PathState load(const QUuid &sessionId) const;
    void      save(const QUuid &sessionId, const PathState &state);

private:
    void flush() const;

    QString                  m_filePath;
    QHash<QUuid, PathState>  m_data;
};

} // namespace linscp::core::session
