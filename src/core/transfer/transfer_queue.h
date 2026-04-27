#pragma once
#include <QObject>
#include <QList>
#include <QMutex>
#include <QUuid>
#include <optional>

#include "transfer_item.h"

namespace linscp::core::transfer {

/// Персистентная очередь передачи файлов.
/// Потокобезопасна — методы защищены мьютексом.
class TransferQueue : public QObject {
    Q_OBJECT
public:
    explicit TransferQueue(QObject *parent = nullptr);

    // ── Управление очередью ───────────────────────────────────────────────────

    QUuid enqueue(TransferItem item);
    void  cancel(const QUuid &id);
    void  pause(const QUuid &id);
    void  resume(const QUuid &id);
    void  clearCompleted();

    QList<TransferItem> items() const;
    TransferItem        item(const QUuid &id) const;

    // ── Вызывается из TransferManager ─────────────────────────────────────────

    void updateProgress(const QUuid &id, qint64 transferred);
    void setTotalBytes(const QUuid &id, qint64 total);
    void setStatus(const QUuid &id, TransferStatus status, const QString &error = {});

    /// Вернуть следующий элемент с статусом Queued, либо null
    std::optional<TransferItem> nextQueued() const;

    // ── Персистентность ───────────────────────────────────────────────────────

    void save(const QString &path) const;
    void load(const QString &path);

signals:
    void itemAdded(const QUuid &id);
    void itemChanged(const QUuid &id);
    void itemRemoved(const QUuid &id);

private:
    mutable QMutex      m_mutex;
    QList<TransferItem> m_items;
};

} // namespace linscp::core::transfer
