#include "copy_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QFileDialog>
#include <QDir>
#include <QDialogButtonBox>

namespace linscp::ui::dialogs {

CopyDialog::CopyDialog(Direction direction,
                       bool isMove,
                       const QStringList &fileList,
                       const QString &defaultTarget,
                       QWidget *parent)
    : QDialog(parent)
    , m_direction(direction)
    , m_isMove(isMove)
    , m_fileList(fileList)
{
    setupUi(defaultTarget);
}

void CopyDialog::setupUi(const QString &defaultTarget)
{
    const bool toRemote = (m_direction == Direction::Upload);
    const QString verb  = m_isMove ? tr("Move") : tr("Copy");

    setWindowTitle(toRemote ? (m_isMove ? tr("Move to Remote") : tr("Copy to Remote"))
                            : (m_isMove ? tr("Move to Local")  : tr("Copy to Local")));
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // Описание файлов
    const QString filesText = m_fileList.size() == 1
        ? tr("%1 to:").arg(m_fileList.first())
        : tr("%n file(s) to:", nullptr, m_fileList.size());
    m_filesLabel = new QLabel(filesText, this);
    layout->addWidget(m_filesLabel);

    // Строка ввода пути назначения + Browse
    auto *dirRow = new QHBoxLayout;
    m_targetEdit = new QLineEdit(defaultTarget, this);
    m_browseBtn  = new QPushButton(tr("Browse…"), this);
    m_browseBtn->setFixedWidth(80);
    dirRow->addWidget(m_targetEdit, 1);
    if (!toRemote)
        dirRow->addWidget(m_browseBtn);
    layout->addLayout(dirRow);

    // Опции
    m_queueCheck = new QCheckBox(tr("Add to &queue"), this);
    layout->addWidget(m_queueCheck);

    layout->addStretch();

    // Кнопки
    auto *btns = new QHBoxLayout;
    m_okBtn     = new QPushButton(verb, this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_okBtn->setDefault(true);
    m_okBtn->setFixedWidth(80);
    m_cancelBtn->setFixedWidth(80);
    btns->addStretch();
    btns->addWidget(m_okBtn);
    btns->addWidget(m_cancelBtn);
    layout->addLayout(btns);

    connect(m_browseBtn,  &QPushButton::clicked, this, &CopyDialog::onBrowse);
    connect(m_okBtn,      &QPushButton::clicked, this, &CopyDialog::onOk);
    connect(m_cancelBtn,  &QPushButton::clicked, this, &QDialog::reject);
}

void CopyDialog::onBrowse()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Destination"), m_targetEdit->text());
    if (!dir.isEmpty())
        m_targetEdit->setText(dir);
}

void CopyDialog::onOk()
{
    m_addToQueue = m_queueCheck->isChecked();
    accept();
}

QString CopyDialog::targetDirectory() const
{
    return m_targetEdit->text();
}

} // namespace linscp::ui::dialogs
