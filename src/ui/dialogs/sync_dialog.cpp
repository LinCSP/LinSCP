#include "sync_dialog.h"
#include "core/sync/sync_engine.h"
#include <QThreadPool>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QStackedWidget>
#include <QLineEdit>
#include <QRadioButton>
#include <QCheckBox>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QtConcurrent>

namespace linscp::ui::dialogs {

SyncDialog::SyncDialog(core::sync::SyncEngine *engine,
                       const QString &localPath,
                       const QString &remotePath,
                       QWidget *parent)
    : QDialog(parent), m_engine(engine)
{
    setupUi(localPath, remotePath);

    connect(engine, &core::sync::SyncEngine::previewReady,
            this, &SyncDialog::onPreviewReady);
    connect(engine, &core::sync::SyncEngine::syncProgress,
            this, &SyncDialog::onSyncProgress);
    connect(engine, &core::sync::SyncEngine::syncFinished,
            this, &SyncDialog::onSyncFinished);
}

void SyncDialog::setupUi(const QString &localPath, const QString &remotePath)
{
    setWindowTitle(tr("Synchronize Directories"));
    setMinimumSize(640, 480);

    m_stack = new QStackedWidget(this);

    // ── Страница 0: настройки ─────────────────────────────────────────────────
    auto *settingsPage = new QWidget;
    auto *sLayout = new QVBoxLayout(settingsPage);
    sLayout->setContentsMargins(12, 12, 12, 12);

    auto *pathBox    = new QGroupBox(tr("Directories"), settingsPage);
    auto *pathLayout = new QFormLayout(pathBox);
    m_localPath  = new QLineEdit(localPath, pathBox);
    m_remotePath = new QLineEdit(remotePath, pathBox);
    pathLayout->addRow(tr("Local:"),  m_localPath);
    pathLayout->addRow(tr("Remote:"), m_remotePath);

    auto *dirBox    = new QGroupBox(tr("Direction"), settingsPage);
    auto *dirLayout = new QVBoxLayout(dirBox);
    m_dirLocal  = new QRadioButton(tr("Local → Remote"), dirBox);
    m_dirRemote = new QRadioButton(tr("Remote → Local"), dirBox);
    m_dirBoth   = new QRadioButton(tr("Bidirectional"),  dirBox);
    m_dirLocal->setChecked(true);
    dirLayout->addWidget(m_dirLocal);
    dirLayout->addWidget(m_dirRemote);
    dirLayout->addWidget(m_dirBoth);

    auto *cmpBox    = new QGroupBox(tr("Compare mode"), settingsPage);
    auto *cmpLayout = new QVBoxLayout(cmpBox);
    m_cmMtime    = new QRadioButton(tr("Time + Size (fast)"),        cmpBox);
    m_cmChecksum = new QRadioButton(tr("Checksum SHA-256 (exact)"),  cmpBox);
    m_cmMtime->setChecked(true);
    cmpLayout->addWidget(m_cmMtime);
    cmpLayout->addWidget(m_cmChecksum);

    auto *filterBox    = new QGroupBox(tr("Filters"), settingsPage);
    auto *filterLayout = new QFormLayout(filterBox);
    m_excludeEdit   = new QLineEdit(filterBox);
    m_excludeEdit->setPlaceholderText("*.tmp, .git, node_modules");
    m_excludeHidden = new QCheckBox(tr("Exclude hidden files"), filterBox);
    filterLayout->addRow(tr("Exclude patterns:"), m_excludeEdit);
    filterLayout->addRow(QString{}, m_excludeHidden);

    sLayout->addWidget(pathBox);
    sLayout->addWidget(dirBox);
    sLayout->addWidget(cmpBox);
    sLayout->addWidget(filterBox);
    sLayout->addStretch();

    // ── Страница 1: preview ───────────────────────────────────────────────────
    auto *previewPage = new QWidget;
    auto *pLayout = new QVBoxLayout(previewPage);

    m_diffSummary = new QLabel(previewPage);
    m_diffTree = new QTreeWidget(previewPage);
    m_diffTree->setColumnCount(4);
    m_diffTree->setHeaderLabels({tr("Action"), tr("File"), tr("Local"), tr("Remote")});
    m_diffTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_diffTree->setAlternatingRowColors(true);
    m_diffTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_diffTree->setSortingEnabled(true);

    pLayout->addWidget(m_diffSummary);
    pLayout->addWidget(m_diffTree, 1);

    // ── Страница 2: прогресс ──────────────────────────────────────────────────
    auto *progressPage = new QWidget;
    auto *prLayout = new QVBoxLayout(progressPage);
    prLayout->setAlignment(Qt::AlignCenter);
    m_progressLabel = new QLabel(tr("Synchronizing…"), progressPage);
    m_progressLabel->setAlignment(Qt::AlignCenter);
    m_progressBar = new QProgressBar(progressPage);
    m_progressBar->setRange(0, 100);
    prLayout->addWidget(m_progressLabel);
    prLayout->addWidget(m_progressBar);

    m_stack->addWidget(settingsPage);
    m_stack->addWidget(previewPage);
    m_stack->addWidget(progressPage);

    // ── Кнопки ────────────────────────────────────────────────────────────────
    m_backBtn    = new QPushButton(tr("← Back"),    this);
    m_previewBtn = new QPushButton(tr("Preview…"),  this);
    m_applyBtn   = new QPushButton(tr("Synchronize"), this);
    m_applyBtn->setEnabled(false);

    auto *closeBtn = new QPushButton(tr("Close"), this);

    connect(m_backBtn,    &QPushButton::clicked, this, [this]() { showPage(0); });
    connect(m_previewBtn, &QPushButton::clicked, this, &SyncDialog::onPreview);
    connect(m_applyBtn,   &QPushButton::clicked, this, &SyncDialog::onApply);
    connect(closeBtn,     &QPushButton::clicked, this, &QDialog::reject);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_backBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_previewBtn);
    btnRow->addWidget(m_applyBtn);
    btnRow->addWidget(closeBtn);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_stack, 1);
    mainLayout->addLayout(btnRow);

    showPage(0);
}

void SyncDialog::showPage(int page)
{
    m_stack->setCurrentIndex(page);
    m_backBtn->setVisible(page > 0);
    m_previewBtn->setVisible(page == 0);
    m_applyBtn->setVisible(page == 1);
}

void SyncDialog::onPreview()
{
    core::sync::SyncProfile profile;
    profile.localPath   = m_localPath->text();
    profile.remotePath  = m_remotePath->text();

    if (m_dirRemote->isChecked())      profile.direction = core::sync::SyncDirection::RemoteToLocal;
    else if (m_dirBoth->isChecked())   profile.direction = core::sync::SyncDirection::Bidirectional;
    else                               profile.direction = core::sync::SyncDirection::LocalToRemote;

    profile.compareMode    = m_cmChecksum->isChecked()
                                 ? core::sync::SyncCompareMode::Checksum
                                 : core::sync::SyncCompareMode::MtimeAndSize;
    profile.excludePatterns = m_excludeEdit->text().split(',', Qt::SkipEmptyParts);
    for (auto &p : profile.excludePatterns) p = p.trimmed();
    profile.excludeHidden   = m_excludeHidden->isChecked();

    m_previewBtn->setEnabled(false);
    m_previewBtn->setText(tr("Scanning…"));

    QThreadPool::globalInstance()->start([this, profile]() {
        const auto diff = m_engine->preview(profile);
        QMetaObject::invokeMethod(this, [this, diff]() {
            emit m_engine->previewReady(diff);
        }, Qt::QueuedConnection);
    });
}

void SyncDialog::onPreviewReady(const QList<core::sync::SyncDiffEntry> &diff)
{
    m_previewBtn->setEnabled(true);
    m_previewBtn->setText(tr("Preview…"));
    m_diff = diff;
    populateDiff(diff);
    showPage(1);
    m_applyBtn->setEnabled(!diff.isEmpty());
}

void SyncDialog::populateDiff(const QList<core::sync::SyncDiffEntry> &diff)
{
    m_diffTree->clear();

    int uploads = 0, downloads = 0, deletes = 0;

    for (const auto &entry : diff) {
        auto *item = new QTreeWidgetItem(m_diffTree);
        switch (entry.action) {
        case core::sync::SyncAction::Upload:
            item->setText(0, tr("↑ Upload"));
            item->setForeground(0, Qt::darkGreen);
            ++uploads;
            break;
        case core::sync::SyncAction::Download:
            item->setText(0, tr("↓ Download"));
            item->setForeground(0, Qt::darkBlue);
            ++downloads;
            break;
        case core::sync::SyncAction::DeleteRemote:
        case core::sync::SyncAction::DeleteLocal:
            item->setText(0, tr("✕ Delete"));
            item->setForeground(0, Qt::darkRed);
            ++deletes;
            break;
        case core::sync::SyncAction::Conflict:
            item->setText(0, tr("⚡ Conflict"));
            item->setForeground(0, Qt::darkYellow);
            break;
        default: break;
        }
        item->setText(1, entry.localPath.isEmpty() ? entry.remotePath
                                                    : entry.localPath);
        item->setText(2, entry.localMtime.toString("yyyy-MM-dd hh:mm"));
        item->setText(3, entry.remoteMtime.toString("yyyy-MM-dd hh:mm"));
    }

    m_diffSummary->setText(
        tr("%1 changes: %2 upload, %3 download, %4 delete")
            .arg(diff.size()).arg(uploads).arg(downloads).arg(deletes));
}

void SyncDialog::onApply()
{
    showPage(2);
    m_applyBtn->setEnabled(false);

    // Профиль реконструируем из текущих данных
    core::sync::SyncProfile profile;
    profile.localPath  = m_localPath->text();
    profile.remotePath = m_remotePath->text();

    QThreadPool::globalInstance()->start([this, profile]() {
        m_engine->apply(profile, m_diff);
    });
}

void SyncDialog::onSyncProgress(int percent)
{
    m_progressBar->setValue(percent);
    m_progressLabel->setText(tr("Synchronizing… %1%").arg(percent));
}

void SyncDialog::onSyncFinished(bool success, const QString &error)
{
    if (success) {
        m_progressLabel->setText(tr("Synchronization complete!"));
    } else {
        m_progressLabel->setText(tr("Error: %1").arg(error));
    }
}

} // namespace linscp::ui::dialogs
