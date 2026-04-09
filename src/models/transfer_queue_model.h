#pragma once
#include <QAbstractTableModel>
#include "core/transfer/transfer_queue.h"

namespace linscp::models {

/// Табличная модель для отображения очереди передач в ui/dialogs/transfer_panel
class TransferQueueModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColDirection = 0,
        ColName,
        ColProgress,
        ColSize,
        ColSpeed,
        ColStatus,
        ColCount
    };

    explicit TransferQueueModel(core::transfer::TransferQueue *queue,
                                QObject *parent = nullptr);

    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private slots:
    void onItemAdded(const QUuid &id);
    void onItemChanged(const QUuid &id);
    void onItemRemoved(const QUuid &id);

private:
    core::transfer::TransferQueue  *m_queue;
    QList<core::transfer::TransferItem> m_snapshot;

    int rowForId(const QUuid &id) const;
};

} // namespace linscp::models
