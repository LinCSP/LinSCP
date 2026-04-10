#include "progress_dialog.h"
#include "core/transfer/transfer_manager.h"
#include "core/transfer/transfer_queue.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QFrame>
#include <QFileInfo>
#include <algorithm>

namespace linscp::ui::dialogs {

using namespace core::transfer;

ProgressDialog::ProgressDialog(TransferManager *manager,
                               TransferQueue   *queue,
                               QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_queue(queue)
{
    setWindowTitle(tr("File Transfer"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumWidth(480);
    setModal(false);   // не блокируем UI — пользователь может работать в панелях

    setupUi();

    connect(m_manager, &TransferManager::transferStarted,
            this, &ProgressDialog::onTransferStarted);
    connect(m_manager, &TransferManager::transferFinished,
            this, &ProgressDialog::onTransferFinished);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(200);
    connect(m_pollTimer, &QTimer::timeout, this, &ProgressDialog::onProgressPoll);
}

// ── Построение UI ─────────────────────────────────────────────────────────────

void ProgressDialog::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(12, 12, 12, 12);

    // Направление + имя файла
    m_dirLabel      = new QLabel(tr("Transferring…"), this);
    m_fileNameLabel = new QLabel(this);
    m_destLabel     = new QLabel(this);
    m_fileNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_destLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QFont bold = m_dirLabel->font();
    bold.setBold(true);
    m_dirLabel->setFont(bold);

    root->addWidget(m_dirLabel);
    root->addWidget(m_fileNameLabel);
    root->addWidget(m_destLabel);

    // Разделитель
    auto *sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep1);

    // Текущий файл
    auto *fileSection = new QGridLayout;
    fileSection->setColumnStretch(1, 1);

    fileSection->addWidget(new QLabel(tr("File:"), this), 0, 0);
    m_fileBar = new QProgressBar(this);
    m_fileBar->setRange(0, 100);
    m_fileBar->setValue(0);
    fileSection->addWidget(m_fileBar, 0, 1);

    m_speedLabel    = new QLabel("–", this);
    m_speedLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fileSection->addWidget(m_speedLabel, 0, 2);

    m_fileSizeLabel = new QLabel("0 B / 0 B", this);
    m_fileSizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fileSection->addWidget(m_fileSizeLabel, 1, 1, 1, 2);

    root->addLayout(fileSection);

    // Общий прогресс
    auto *totalSection = new QGridLayout;
    totalSection->setColumnStretch(1, 1);

    totalSection->addWidget(new QLabel(tr("Total:"), this), 0, 0);
    m_totalBar = new QProgressBar(this);
    m_totalBar->setRange(0, 100);
    m_totalBar->setValue(0);
    totalSection->addWidget(m_totalBar, 0, 1);

    m_etaLabel = new QLabel("ETA: –", this);
    m_etaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totalSection->addWidget(m_etaLabel, 0, 2);

    m_totalCountLabel = new QLabel("0 / 0 files", this);
    m_totalCountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totalSection->addWidget(m_totalCountLabel, 1, 1, 1, 2);

    root->addLayout(totalSection);

    // Разделитель
    auto *sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep2);

    // Кнопки
    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    m_bgBtn = new QPushButton(tr("Background"), this);
    m_bgBtn->setToolTip(tr("Hide this dialog; transfer continues in the queue"));
    btnLayout->addWidget(m_bgBtn);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setDefault(true);
    btnLayout->addWidget(m_cancelBtn);

    root->addLayout(btnLayout);

    connect(m_bgBtn,     &QPushButton::clicked, this, &ProgressDialog::onBackgroundClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &ProgressDialog::onCancelClicked);
}

// ── Публичный API ─────────────────────────────────────────────────────────────

void ProgressDialog::trackTransfer(const QUuid &id)
{
    m_trackedId = id;

    const TransferItem item = m_queue->item(id);
    if (item.id.isNull()) return;

    ++m_totalFiles;
    refreshFromItem(item);
    m_pollTimer->start();

    if (!isVisible()) show();
}

// ── Слоты ─────────────────────────────────────────────────────────────────────

void ProgressDialog::onTransferStarted(const QUuid &id)
{
    // Если ещё ни за чем не следим — начать следить
    if (m_trackedId.isNull())
        trackTransfer(id);
}

void ProgressDialog::onTransferFinished(const QUuid &id, bool /*success*/)
{
    if (id == m_trackedId) {
        ++m_doneFiles;
        m_trackedId = QUuid();
    }

    // Проверяем — есть ли ещё активные/ожидающие задания
    const auto allItems = m_queue->items();
    const bool hasMore  = std::any_of(allItems.cbegin(), allItems.cend(),
        [](const TransferItem &it) {
            return it.status == TransferStatus::InProgress ||
                   it.status == TransferStatus::Queued;
        });
    if (!hasMore) {
        m_pollTimer->stop();
        accept();   // авто-закрытие при завершении всех заданий
    }
}

void ProgressDialog::onProgressPoll()
{
    if (m_trackedId.isNull()) {
        // Попробовать взять следующее активное задание
        for (const auto &it : m_queue->items()) {
            if (it.status == TransferStatus::InProgress) {
                m_trackedId = it.id;
                break;
            }
        }
        if (m_trackedId.isNull()) return;
    }

    const TransferItem item = m_queue->item(m_trackedId);
    if (item.id.isNull()) return;

    refreshFromItem(item);
}

void ProgressDialog::onCancelClicked()
{
    if (!m_trackedId.isNull())
        m_queue->cancel(m_trackedId);
    m_pollTimer->stop();
    reject();
}

void ProgressDialog::onBackgroundClicked()
{
    m_pollTimer->stop();
    hide();
    emit movedToBackground();
}

// ── Вспомогательные ──────────────────────────────────────────────────────────

void ProgressDialog::refreshFromItem(const TransferItem &item)
{
    // Направление
    const bool upload = item.direction == TransferDirection::Upload;
    m_dirLabel->setText(upload ? tr("↑ Uploading") : tr("↓ Downloading"));

    // Имя файла
    const QString name = upload
        ? QFileInfo(item.localPath).fileName()
        : QFileInfo(item.remotePath).fileName();
    m_fileNameLabel->setText(tr("File: <b>%1</b>").arg(name.toHtmlEscaped()));

    const QString dest = upload ? item.remotePath : item.localPath;
    m_destLabel->setText(tr("To: %1").arg(dest.toHtmlEscaped()));

    // Текущий файл прогресс
    m_fileBar->setValue(item.percent());
    m_fileSizeLabel->setText(
        tr("%1 / %2")
            .arg(formatBytes(item.transferredBytes),
                 formatBytes(item.totalBytes)));

    // Скорость
    const double bps = item.speedBps();
    if (bps > 0)
        m_speedLabel->setText(formatBytes(static_cast<qint64>(bps)) + "/s");
    else
        m_speedLabel->setText("–");

    // Общий прогресс
    if (m_totalFiles > 0) {
        const int totalPct = (m_doneFiles * 100 + item.percent()) / m_totalFiles;
        m_totalBar->setValue(qMin(totalPct, 100));
        m_totalCountLabel->setText(
            tr("%1 / %2 files").arg(m_doneFiles + 1).arg(m_totalFiles));
    }

    // ETA
    if (bps > 1.0 && item.totalBytes > item.transferredBytes) {
        const int etaSecs = static_cast<int>(
            (item.totalBytes - item.transferredBytes) / bps);
        m_etaLabel->setText(tr("ETA: %1").arg(formatEta(etaSecs)));
    } else {
        m_etaLabel->setText("ETA: –");
    }
}

QString ProgressDialog::formatBytes(qint64 bytes) const
{
    if (bytes < 1024)
        return tr("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return tr("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return tr("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return tr("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

QString ProgressDialog::formatEta(int seconds) const
{
    if (seconds < 0) return "–";
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0'))
                                         .arg(s, 2, 10, QLatin1Char('0'));
    return QString("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

} // namespace linscp::ui::dialogs
