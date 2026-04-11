#include "key_dialog.h"
#include "ui/utils/svg_icon.h"
#include "core/keys/key_manager.h"
#include "core/keys/key_generator.h"
#include "core/keys/ppk_converter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>

namespace linscp::ui::dialogs {

KeyDialog::KeyDialog(core::keys::KeyManager *keyMgr,
                     core::keys::KeyGenerator *keyGen, QWidget *parent)
    : QDialog(parent), m_keyMgr(keyMgr), m_keyGen(keyGen)
{
    setupUi();
    refreshList();
}

void KeyDialog::setupUi()
{
    setWindowTitle(tr("SSH Key Manager"));
    setMinimumSize(560, 400);

    // Список ключей
    m_list = new QListWidget(this);
    connect(m_list, &QListWidget::currentRowChanged,
            this, &KeyDialog::onSelectionChanged);

    // Детали выбранного ключа
    auto *detailBox    = new QGroupBox(tr("Key details"), this);
    auto *detailLayout = new QFormLayout(detailBox);
    m_typeLabel        = new QLabel(detailBox);
    m_fingerprintLabel = new QLabel(detailBox);
    m_fingerprintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_fingerprintLabel->setWordWrap(true);
    detailLayout->addRow(tr("Type:"),        m_typeLabel);
    detailLayout->addRow(tr("Fingerprint:"), m_fingerprintLabel);

    // Кнопки управления
    m_addBtn     = new QPushButton(svgIcon(QStringLiteral("circle-plus")),    tr("Add…"),     this);
    m_genBtn     = new QPushButton(svgIcon(QStringLiteral("key")),  tr("Generate…"),this);
    m_convertBtn = new QPushButton(tr("Convert PPK…"), this);
    m_removeBtn  = new QPushButton(svgIcon(QStringLiteral("circle-minus")), tr("Remove"),   this);
    m_removeBtn->setEnabled(false);

    connect(m_addBtn,     &QPushButton::clicked, this, &KeyDialog::onAddKey);
    connect(m_genBtn,     &QPushButton::clicked, this, &KeyDialog::onGenerateKey);
    connect(m_convertBtn, &QPushButton::clicked, this, &KeyDialog::onConvertKey);
    connect(m_removeBtn,  &QPushButton::clicked, this, &KeyDialog::onRemoveKey);

    auto *btnLayout = new QVBoxLayout;
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_genBtn);
    btnLayout->addWidget(m_convertBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_removeBtn);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(m_list, 1);
    topRow->addLayout(btnLayout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *main = new QVBoxLayout(this);
    main->addLayout(topRow);
    main->addWidget(detailBox);
    main->addWidget(buttons);
}

void KeyDialog::refreshList()
{
    const int prev = m_list->currentRow();
    m_list->clear();

    for (const auto &info : m_keyMgr->defaultKeys()) {
        auto *item = new QListWidgetItem(info.path, m_list);
        item->setData(Qt::UserRole, info.fingerprint);
        item->setData(Qt::UserRole + 1, static_cast<int>(info.type));
        if (info.hasPassphrase)
            item->setIcon(svgIcon(QStringLiteral("lock")));
        else
            item->setIcon(svgIcon(QStringLiteral("shield-halved")));
    }

    m_list->setCurrentRow(prev);
}

void KeyDialog::onSelectionChanged()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item) {
        m_typeLabel->clear();
        m_fingerprintLabel->clear();
        m_removeBtn->setEnabled(false);
        return;
    }
    m_removeBtn->setEnabled(true);
    m_fingerprintLabel->setText(item->data(Qt::UserRole).toString());
    switch (static_cast<core::keys::KeyType>(item->data(Qt::UserRole + 1).toInt())) {
    case core::keys::KeyType::RSA:     m_typeLabel->setText("RSA");     break;
    case core::keys::KeyType::ED25519: m_typeLabel->setText("ED25519"); break;
    case core::keys::KeyType::ECDSA:   m_typeLabel->setText("ECDSA");   break;
    default:                           m_typeLabel->setText("Unknown"); break;
    }
}

void KeyDialog::onAddKey()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Add SSH Key"),
        QDir::homePath() + "/.ssh",
        tr("Key files (id_rsa id_ed25519 id_ecdsa *.pem *.ppk);;All files (*)"));
    if (path.isEmpty()) return;
    // KeyManager уже обращается к defaultKeys(); достаточно показать путь.
    auto *item = new QListWidgetItem(path, m_list);
    item->setIcon(svgIcon(QStringLiteral("shield-halved")));
}

void KeyDialog::onGenerateKey()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Generate Key Pair"));
    auto *layout = new QVBoxLayout(&dlg);
    auto *form   = new QFormLayout;

    auto *typeBox = new QComboBox(&dlg);
    typeBox->addItem("ED25519",  static_cast<int>(core::keys::GenerateKeyType::ED25519));
    typeBox->addItem("RSA 4096", static_cast<int>(core::keys::GenerateKeyType::RSA4096));
    typeBox->addItem("RSA 2048", static_cast<int>(core::keys::GenerateKeyType::RSA2048));
    typeBox->addItem("ECDSA 256",static_cast<int>(core::keys::GenerateKeyType::ECDSA256));

    auto *pathEdit = new QLineEdit(QDir::homePath() + "/.ssh/id_linscp", &dlg);
    auto *passEdit = new QLineEdit(&dlg);
    passEdit->setEchoMode(QLineEdit::Password);
    passEdit->setPlaceholderText(tr("Optional passphrase"));

    form->addRow(tr("Type:"),       typeBox);
    form->addRow(tr("Save to:"),    pathEdit);
    form->addRow(tr("Passphrase:"), passEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    const auto keyType = static_cast<core::keys::GenerateKeyType>(
        typeBox->currentData().toInt());

    connect(m_keyGen, &core::keys::KeyGenerator::generated, this, [this](const QString &priv, const QString &) {
        QMessageBox::information(this, tr("Key Generated"),
                                  tr("Key saved: %1").arg(priv));
        refreshList();
    });
    m_keyGen->generate(keyType, pathEdit->text(), passEdit->text());
}

void KeyDialog::onConvertKey()
{
    const QString ppk = QFileDialog::getOpenFileName(
        this, tr("Select .ppk file"), QDir::homePath() + "/.ssh",
        tr("PuTTY keys (*.ppk)"));
    if (ppk.isEmpty()) return;

    const QString out = QFileDialog::getSaveFileName(
        this, tr("Save OpenSSH key"), ppk.chopped(4),
        tr("OpenSSH key (*)"));
    if (out.isEmpty()) return;

    if (core::keys::PpkConverter::ppkToOpenSsh(ppk, out))
        QMessageBox::information(this, tr("Converted"), tr("Saved: %1").arg(out));
    else
        QMessageBox::warning(this, tr("Error"), core::keys::PpkConverter::lastError());
}

void KeyDialog::onRemoveKey()
{
    const int row = m_list->currentRow();
    if (row < 0) return;
    delete m_list->takeItem(row);
}

} // namespace linscp::ui::dialogs
