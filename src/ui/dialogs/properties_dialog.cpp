#include "properties_dialog.h"
#include "core/i_remote_file_system.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QLocale>

namespace linscp::ui::dialogs {

PropertiesDialog::PropertiesDialog(const core::sftp::SftpFileInfo &fileInfo,
                                   core::IRemoteFileSystem *fs,
                                   QWidget *parent)
    : QDialog(parent)
    , m_info(fileInfo)
    , m_fs(fs)
{
    setupUi();
    populateInfo();
}

void PropertiesDialog::setupUi()
{
    setWindowTitle(tr("Properties — %1").arg(m_info.name));
    setMinimumWidth(400);

    auto *mainLayout = new QVBoxLayout(this);

    auto *tabs = new QTabWidget(this);
    mainLayout->addWidget(tabs);

    // ── Вкладка "General" ────────────────────────────────────────────────────
    auto *generalWidget = new QWidget;
    auto *genForm = new QFormLayout(generalWidget);
    genForm->setSpacing(6);

    m_nameEdit    = new QLineEdit(m_info.name, generalWidget);
    m_nameEdit->setReadOnly(true);   // rename через F6, не здесь
    m_locationLbl = new QLabel(generalWidget);
    m_sizeLbl     = new QLabel(generalWidget);
    m_dateLbl     = new QLabel(generalWidget);
    m_ownerLbl    = new QLabel(generalWidget);
    m_groupLbl    = new QLabel(generalWidget);

    genForm->addRow(tr("Name:"),     m_nameEdit);
    genForm->addRow(tr("Location:"), m_locationLbl);
    genForm->addRow(tr("Size:"),     m_sizeLbl);
    genForm->addRow(tr("Modified:"), m_dateLbl);
    genForm->addRow(tr("Owner:"),    m_ownerLbl);
    genForm->addRow(tr("Group:"),    m_groupLbl);

    tabs->addTab(generalWidget, tr("General"));

    // ── Вкладка "Permissions" — аналог WinSCP Rights frame ──────────────────
    auto *permWidget = new QWidget;
    auto *permVLayout = new QVBoxLayout(permWidget);

    auto *permGroup = new QGroupBox(tr("Permissions"), permWidget);
    auto *grid = new QGridLayout(permGroup);

    // Заголовки колонок
    const QStringList colHeaders = { tr("Read"), tr("Write"), tr("Execute") };
    // Заголовки строк
    const QStringList rowHeaders = { tr("Owner"), tr("Group"), tr("Other") };

    for (int col = 0; col < 3; ++col)
        grid->addWidget(new QLabel(colHeaders[col], permGroup), 0, col + 1,
                        Qt::AlignCenter);

    for (int row = 0; row < 3; ++row) {
        grid->addWidget(new QLabel(rowHeaders[row], permGroup), row + 1, 0);
        for (int col = 0; col < 3; ++col) {
            m_perm[row][col] = new QCheckBox(permGroup);
            connect(m_perm[row][col], &QCheckBox::toggled,
                    this, &PropertiesDialog::onPermToggled);
            grid->addWidget(m_perm[row][col], row + 1, col + 1, Qt::AlignCenter);
        }
    }

    // Sticky/SUID/SGID строка
    m_stickyChk = new QCheckBox(tr("Sticky / Special bits"), permGroup);
    connect(m_stickyChk, &QCheckBox::toggled,
            this, &PropertiesDialog::onPermToggled);
    grid->addWidget(m_stickyChk, 4, 0, 1, 4);

    permVLayout->addWidget(permGroup);

    // Поле с восьмеричным представлением (read-only, обновляется при клике)
    auto *octalRow = new QHBoxLayout;
    octalRow->addWidget(new QLabel(tr("Octal:"), permWidget));
    m_octalLbl = new QLabel(permWidget);
    m_octalLbl->setFont(QFont("monospace"));
    octalRow->addWidget(m_octalLbl);
    octalRow->addStretch();
    permVLayout->addLayout(octalRow);
    permVLayout->addStretch();

    tabs->addTab(permWidget, tr("Permissions"));

    // ── Кнопки ───────────────────────────────────────────────────────────────
    auto *btnLayout = new QHBoxLayout;
    m_applyBtn = new QPushButton(tr("Apply"), this);
    auto *closeBtn = new QPushButton(tr("Close"), this);
    m_applyBtn->setEnabled(false);
    btnLayout->addStretch();
    btnLayout->addWidget(m_applyBtn);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_applyBtn, &QPushButton::clicked, this, &PropertiesDialog::onApply);
    connect(closeBtn,   &QPushButton::clicked, this, &QDialog::accept);
}

void PropertiesDialog::populateInfo()
{
    const QString path = m_info.path.section('/', 0, -2);
    m_locationLbl->setText(path.isEmpty() ? "/" : path);
    m_sizeLbl->setText(m_info.isDir
        ? tr("—")
        : QLocale().formattedDataSize(m_info.size));
    m_dateLbl->setText(m_info.mtime.toString(Qt::RFC2822Date));
    m_ownerLbl->setText(m_info.owner.isEmpty() ? tr("—") : m_info.owner);
    m_groupLbl->setText(m_info.group.isEmpty() ? tr("—") : m_info.group);

    // Заполняем чекбоксы прав
    const uint perm = m_info.permissions;
    // owner
    m_perm[0][0]->setChecked(perm & 0400);  // S_IRUSR
    m_perm[0][1]->setChecked(perm & 0200);  // S_IWUSR
    m_perm[0][2]->setChecked(perm & 0100);  // S_IXUSR
    // group
    m_perm[1][0]->setChecked(perm & 0040);  // S_IRGRP
    m_perm[1][1]->setChecked(perm & 0020);  // S_IWGRP
    m_perm[1][2]->setChecked(perm & 0010);  // S_IXGRP
    // other
    m_perm[2][0]->setChecked(perm & 0004);  // S_IROTH
    m_perm[2][1]->setChecked(perm & 0002);  // S_IWOTH
    m_perm[2][2]->setChecked(perm & 0001);  // S_IXOTH
    // sticky / suid / sgid
    m_stickyChk->setChecked(perm & 07000);

    onPermToggled();  // обновить octal label
}

uint PropertiesDialog::collectPermissions() const
{
    uint p = 0;
    if (m_perm[0][0]->isChecked()) p |= 0400;
    if (m_perm[0][1]->isChecked()) p |= 0200;
    if (m_perm[0][2]->isChecked()) p |= 0100;
    if (m_perm[1][0]->isChecked()) p |= 0040;
    if (m_perm[1][1]->isChecked()) p |= 0020;
    if (m_perm[1][2]->isChecked()) p |= 0010;
    if (m_perm[2][0]->isChecked()) p |= 0004;
    if (m_perm[2][1]->isChecked()) p |= 0002;
    if (m_perm[2][2]->isChecked()) p |= 0001;
    if (m_stickyChk->isChecked())  p |= (m_info.permissions & 07000);
    return p;
}

void PropertiesDialog::onPermToggled()
{
    const uint p = collectPermissions();
    m_octalLbl->setText(QString::number(p, 8).rightJustified(4, '0'));
    m_applyBtn->setEnabled(p != (m_info.permissions & 07777));
}

void PropertiesDialog::onApply()
{
    if (!m_fs) return;
    const uint newPerm = collectPermissions();
    if (m_fs->chmod(m_info.path, newPerm)) {
        m_info.permissions = newPerm;
        m_applyBtn->setEnabled(false);
    }
}

} // namespace linscp::ui::dialogs
