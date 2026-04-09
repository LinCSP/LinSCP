#include "transfer_panel.h"
#include "models/transfer_queue_model.h"
#include "core/transfer/transfer_queue.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>

namespace linscp::ui::dialogs {

TransferPanel::TransferPanel(core::transfer::TransferQueue *queue, QWidget *parent)
    : QWidget(parent), m_queue(queue)
{
    m_model = new models::TransferQueueModel(queue, this);
    setupUi();

    connect(queue, &core::transfer::TransferQueue::itemChanged, this, [this]() {
        int active = 0, queued = 0;
        for (const auto &it : m_queue->items()) {
            if (it.status == core::transfer::TransferStatus::InProgress) ++active;
            if (it.status == core::transfer::TransferStatus::Queued)     ++queued;
        }
        m_summaryLabel->setText(tr("%1 active, %2 queued").arg(active).arg(queued));
    });
}

void TransferPanel::setupUi()
{
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->horizontalHeader()->setSectionResizeMode(
        models::TransferQueueModel::ColName, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(false);

    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &TransferPanel::onSelectionChanged);

    m_pauseBtn  = new QPushButton(tr("Pause"),  this);
    m_resumeBtn = new QPushButton(tr("Resume"), this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_clearBtn  = new QPushButton(tr("Clear completed"), this);
    m_summaryLabel = new QLabel(this);

    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    m_cancelBtn->setEnabled(false);

    connect(m_pauseBtn,  &QPushButton::clicked, this, &TransferPanel::onPause);
    connect(m_resumeBtn, &QPushButton::clicked, this, &TransferPanel::onResume);
    connect(m_cancelBtn, &QPushButton::clicked, this, &TransferPanel::onCancel);
    connect(m_clearBtn,  &QPushButton::clicked, this, &TransferPanel::onClearCompleted);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_pauseBtn);
    btnRow->addWidget(m_resumeBtn);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_summaryLabel);
    btnRow->addWidget(m_clearBtn);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_table, 1);
    layout->addLayout(btnRow);
}

void TransferPanel::onSelectionChanged()
{
    const bool any = m_table->selectionModel()->hasSelection();
    m_pauseBtn->setEnabled(any);
    m_resumeBtn->setEnabled(any);
    m_cancelBtn->setEnabled(any);
}

void TransferPanel::onPause()
{
    for (const QModelIndex &idx : m_table->selectionModel()->selectedRows()) {
        const QUuid id = QUuid::fromString(
            idx.data(Qt::UserRole).toString());
        m_queue->pause(id);
    }
}

void TransferPanel::onResume()
{
    for (const QModelIndex &idx : m_table->selectionModel()->selectedRows()) {
        const QUuid id = QUuid::fromString(idx.data(Qt::UserRole).toString());
        m_queue->resume(id);
    }
}

void TransferPanel::onCancel()
{
    for (const QModelIndex &idx : m_table->selectionModel()->selectedRows()) {
        const QUuid id = QUuid::fromString(idx.data(Qt::UserRole).toString());
        m_queue->cancel(id);
    }
}

void TransferPanel::onClearCompleted()
{
    m_queue->clearCompleted();
}

} // namespace linscp::ui::dialogs
