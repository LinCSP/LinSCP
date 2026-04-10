#include "transfer_queue.h"
#include <QMutexLocker>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

namespace linscp::core::transfer {

TransferQueue::TransferQueue(QObject *parent) : QObject(parent) {}

QUuid TransferQueue::enqueue(TransferItem item)
{
    const QUuid id = item.id;
    {
        QMutexLocker lock(&m_mutex);
        m_items.append(std::move(item));
    }
    // Эмитируем ПОСЛЕ освобождения мьютекса — иначе дедлок если слот
    // вызывает items() / nextQueued() из того же потока
    emit itemAdded(id);
    return id;
}

void TransferQueue::cancel(const QUuid &id)
{
    setStatus(id, TransferStatus::Cancelled);
}

void TransferQueue::pause(const QUuid &id)
{
    setStatus(id, TransferStatus::Paused);
}

void TransferQueue::resume(const QUuid &id)
{
    setStatus(id, TransferStatus::Queued);
}

void TransferQueue::clearCompleted()
{
    QList<QUuid> removed;
    {
        QMutexLocker lock(&m_mutex);
        m_items.removeIf([&removed](const TransferItem &it) {
            if (it.status == TransferStatus::Completed ||
                it.status == TransferStatus::Cancelled) {
                removed << it.id;
                return true;
            }
            return false;
        });
    }
    for (const auto &id : removed) emit itemRemoved(id);
}

QList<TransferItem> TransferQueue::items() const
{
    QMutexLocker lock(&m_mutex);
    return m_items;
}

TransferItem TransferQueue::item(const QUuid &id) const
{
    QMutexLocker lock(&m_mutex);
    for (const auto &it : m_items)
        if (it.id == id) return it;
    return {};
}

std::optional<TransferItem> TransferQueue::nextQueued() const
{
    QMutexLocker lock(&m_mutex);
    for (const auto &it : m_items)
        if (it.status == TransferStatus::Queued) return it;
    return std::nullopt;
}

void TransferQueue::updateProgress(const QUuid &id, qint64 transferred)
{
    {
        QMutexLocker lock(&m_mutex);
        for (auto &it : m_items) {
            if (it.id == id) {
                it.transferredBytes = transferred;
                break;
            }
        }
    }
    emit itemChanged(id);
}

void TransferQueue::setStatus(const QUuid &id, TransferStatus status, const QString &error)
{
    {
        QMutexLocker lock(&m_mutex);
        for (auto &it : m_items) {
            if (it.id == id) {
                it.status = status;
                if (!error.isEmpty()) it.errorMessage = error;
                if (status == TransferStatus::InProgress && !it.startedAt.isValid())
                    it.startedAt = QDateTime::currentDateTime();
                if (status == TransferStatus::Completed || status == TransferStatus::Failed)
                    it.finishedAt = QDateTime::currentDateTime();
                break;
            }
        }
    }
    emit itemChanged(id);
}

void TransferQueue::save(const QString &path) const
{
    QMutexLocker lock(&m_mutex);
    QJsonArray arr;
    for (const auto &it : m_items) {
        if (it.status == TransferStatus::Completed ||
            it.status == TransferStatus::Cancelled) continue;
        QJsonObject obj;
        obj["id"]          = it.id.toString();
        obj["direction"]   = static_cast<int>(it.direction);
        obj["localPath"]   = it.localPath;
        obj["remotePath"]  = it.remotePath;
        obj["totalBytes"]  = it.totalBytes;
        obj["transferred"] = it.transferredBytes;
        arr.append(obj);
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson());
}

void TransferQueue::load(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    QMutexLocker lock(&m_mutex);
    for (const auto &v : arr) {
        const QJsonObject obj = v.toObject();
        TransferItem it;
        it.id               = QUuid::fromString(obj["id"].toString());
        it.direction        = static_cast<TransferDirection>(obj["direction"].toInt());
        it.localPath        = obj["localPath"].toString();
        it.remotePath       = obj["remotePath"].toString();
        it.totalBytes       = obj["totalBytes"].toInteger();
        it.resumeOffset     = obj["transferred"].toInteger();
        it.status           = TransferStatus::Queued;
        m_items.append(it);
    }
}

} // namespace linscp::core::transfer
