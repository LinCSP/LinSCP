#include "overwrite_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QIcon>
#include <QFileInfo>
#include <QInputDialog>
#include <QLocale>

namespace linscp::ui::dialogs {

OverwriteDialog::OverwriteDialog(const FileInfo &source,
                                 const FileInfo &target,
                                 QWidget *parent)
    : QDialog(parent)
{
    setupUi(source, target);
}

void OverwriteDialog::setupUi(const FileInfo &source, const FileInfo &target)
{
    setWindowTitle(tr("File Conflict"));
    setWindowIcon(QIcon::fromTheme("dialog-warning"));
    setMinimumWidth(500);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // Иконка + описание
    auto *topRow = new QHBoxLayout;
    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon::fromTheme("dialog-warning")
                             .pixmap(QSize(48, 48)));
    topRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto *msgLabel = new QLabel(
        tr("<b>The target file already exists.</b><br>"
           "What would you like to do?"), this);
    msgLabel->setWordWrap(true);
    topRow->addWidget(msgLabel, 1);
    layout->addLayout(topRow);

    // Сравнение файлов
    auto *box = new QGroupBox(this);
    auto *grid = new QGridLayout(box);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    grid->addWidget(new QLabel(tr(""), this),    0, 0);
    grid->addWidget(new QLabel(tr("Source"),   this), 0, 1, Qt::AlignCenter);
    grid->addWidget(new QLabel(tr("Target"),   this), 0, 2, Qt::AlignCenter);

    grid->addWidget(new QLabel(tr("Name:"),    this), 1, 0);
    grid->addWidget(new QLabel(QFileInfo(source.path).fileName(), this), 1, 1);
    grid->addWidget(new QLabel(QFileInfo(target.path).fileName(), this), 1, 2);

    grid->addWidget(new QLabel(tr("Size:"),    this), 2, 0);
    grid->addWidget(new QLabel(source.size >= 0
        ? tr("%1 bytes").arg(source.size) : tr("unknown"), this), 2, 1);
    grid->addWidget(new QLabel(target.size >= 0
        ? tr("%1 bytes").arg(target.size) : tr("unknown"), this), 2, 2);

    grid->addWidget(new QLabel(tr("Modified:"), this), 3, 0);
    grid->addWidget(new QLabel(source.modified.isValid()
        ? QLocale().toString(source.modified, QLocale::ShortFormat) : tr("—"), this), 3, 1);
    grid->addWidget(new QLabel(target.modified.isValid()
        ? QLocale().toString(target.modified, QLocale::ShortFormat) : tr("—"), this), 3, 2);

    layout->addWidget(box);

    // Кнопки действий
    auto *btnLayout = new QGridLayout;
    btnLayout->setSpacing(6);

    auto *overwriteBtn    = new QPushButton(tr("Overwrite"),      this);
    auto *overwriteAllBtn = new QPushButton(tr("Overwrite All"),  this);
    auto *skipBtn         = new QPushButton(tr("Skip"),           this);
    auto *skipAllBtn      = new QPushButton(tr("Skip All"),       this);
    auto *renameBtn       = new QPushButton(tr("Rename…"),        this);
    auto *cancelBtn       = new QPushButton(tr("Cancel"),         this);

    overwriteBtn->setDefault(true);
    cancelBtn->setAutoDefault(false);

    btnLayout->addWidget(overwriteBtn,    0, 0);
    btnLayout->addWidget(overwriteAllBtn, 0, 1);
    btnLayout->addWidget(skipBtn,         1, 0);
    btnLayout->addWidget(skipAllBtn,      1, 1);
    btnLayout->addWidget(renameBtn,       2, 0);
    btnLayout->addWidget(cancelBtn,       2, 1);

    layout->addLayout(btnLayout);

    connect(overwriteBtn,    &QPushButton::clicked, this, &OverwriteDialog::onOverwrite);
    connect(overwriteAllBtn, &QPushButton::clicked, this, &OverwriteDialog::onOverwriteAll);
    connect(skipBtn,         &QPushButton::clicked, this, &OverwriteDialog::onSkip);
    connect(skipAllBtn,      &QPushButton::clicked, this, &OverwriteDialog::onSkipAll);
    connect(renameBtn,       &QPushButton::clicked, this, &OverwriteDialog::onRename);
    connect(cancelBtn,       &QPushButton::clicked, this, &OverwriteDialog::onCancel);
}

// ── Слоты ─────────────────────────────────────────────────────────────────────

void OverwriteDialog::onOverwrite()
{
    m_action = Action::Overwrite;
    accept();
}

void OverwriteDialog::onOverwriteAll()
{
    m_action = Action::OverwriteAll;
    accept();
}

void OverwriteDialog::onSkip()
{
    m_action = Action::Skip;
    accept();
}

void OverwriteDialog::onSkipAll()
{
    m_action = Action::SkipAll;
    accept();
}

void OverwriteDialog::onRename()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename Target"),
        tr("New name for the target file:"),
        QLineEdit::Normal, QString{}, &ok);
    if (!ok || name.isEmpty()) return;
    m_action  = Action::Rename;
    m_newName = name;
    accept();
}

void OverwriteDialog::onCancel()
{
    m_action = Action::Cancel;
    reject();
}

} // namespace linscp::ui::dialogs
