#include "transfer_queue_model.h"
#include <QFileInfo>

namespace linscp::models {

using namespace core::transfer;

TransferQueueModel::TransferQueueModel(TransferQueue *queue, QObject *parent)
    : QAbstractTableModel(parent), m_queue(queue)
{
    m_snapshot = queue->items();

    connect(queue, &TransferQueue::itemAdded,   this, &TransferQueueModel::onItemAdded);
    connect(queue, &TransferQueue::itemChanged, this, &TransferQueueModel::onItemChanged);
    connect(queue, &TransferQueue::itemRemoved, this, &TransferQueueModel::onItemRemoved);
}

int TransferQueueModel::rowCount(const QModelIndex &) const
{
    return m_snapshot.size();
}

int TransferQueueModel::columnCount(const QModelIndex &) const { return ColCount; }

QVariant TransferQueueModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_snapshot.size()) return {};
    const auto &item = m_snapshot[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColDirection:
            return item.direction == TransferDirection::Upload ? tr("↑ Upload") : tr("↓ Download");
        case ColName:
            return QFileInfo(item.localPath).fileName();
        case ColProgress:
            return QStringLiteral("%1%").arg(item.percent());
        case ColSize: {
            const qint64 sz = item.totalBytes;
            if (sz < 1024)          return QStringLiteral("%1 B").arg(sz);
            if (sz < 1024 * 1024)   return QStringLiteral("%1 KB").arg(sz / 1024);
            return QStringLiteral("%1 MB").arg(sz / (1024 * 1024));
        }
        case ColSpeed: {
            const double bps = item.speedBps();
            if (bps < 1024)        return QStringLiteral("%1 B/s").arg(bps, 0, 'f', 0);
            if (bps < 1024 * 1024) return QStringLiteral("%1 KB/s").arg(bps / 1024, 0, 'f', 1);
            return QStringLiteral("%1 MB/s").arg(bps / (1024 * 1024), 0, 'f', 2);
        }
        case ColStatus:
            switch (item.status) {
            case TransferStatus::Queued:     return tr("Queued");
            case TransferStatus::InProgress: return tr("Transferring");
            case TransferStatus::Paused:     return tr("Paused");
            case TransferStatus::Completed:  return tr("Done");
            case TransferStatus::Failed:     return tr("Failed");
            case TransferStatus::Cancelled:  return tr("Cancelled");
            }
        }
    }

    if (role == Qt::UserRole) return item.id.toString(); // для поиска строки

    return {};
}

QVariant TransferQueueModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case ColDirection: return tr("Dir");
    case ColName:      return tr("File");
    case ColProgress:  return tr("%");
    case ColSize:      return tr("Size");
    case ColSpeed:     return tr("Speed");
    case ColStatus:    return tr("Status");
    }
    return {};
}

int TransferQueueModel::rowForId(const QUuid &id) const
{
    for (int i = 0; i < m_snapshot.size(); ++i)
        if (m_snapshot[i].id == id) return i;
    return -1;
}

void TransferQueueModel::onItemAdded(const QUuid &id)
{
    const auto item = m_queue->item(id);
    beginInsertRows({}, m_snapshot.size(), m_snapshot.size());
    m_snapshot.append(item);
    endInsertRows();
}

void TransferQueueModel::onItemChanged(const QUuid &id)
{
    const int row = rowForId(id);
    if (row < 0) return;
    m_snapshot[row] = m_queue->item(id);
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void TransferQueueModel::onItemRemoved(const QUuid &id)
{
    const int row = rowForId(id);
    if (row < 0) return;
    beginRemoveRows({}, row, row);
    m_snapshot.removeAt(row);
    endRemoveRows();
}

} // namespace linscp::models
