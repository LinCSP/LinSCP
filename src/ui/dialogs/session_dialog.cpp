#include "session_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>

namespace linscp::ui::dialogs {

SessionDialog::SessionDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

SessionDialog::SessionDialog(const core::session::SessionProfile &profile, QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populate(profile);
}

void SessionDialog::setupUi()
{
    setWindowTitle(tr("Session Properties"));
    setMinimumWidth(480);

    auto *tabs = new QTabWidget(this);

    // ── Tab: General ─────────────────────────────────────────────────────────
    auto *generalTab    = new QWidget;
    auto *generalLayout = new QFormLayout(generalTab);
    generalLayout->setContentsMargins(12, 12, 12, 12);
    generalLayout->setSpacing(8);

    m_name     = new QLineEdit(generalTab);
    m_host     = new QLineEdit(generalTab);
    m_port     = new QSpinBox(generalTab);
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    m_username = new QLineEdit(generalTab);
    m_initialRemote = new QLineEdit(generalTab);
    m_initialRemote->setText("/");

    generalLayout->addRow(tr("Name:"),           m_name);
    generalLayout->addRow(tr("Host:"),           m_host);
    generalLayout->addRow(tr("Port:"),           m_port);
    generalLayout->addRow(tr("Username:"),       m_username);
    generalLayout->addRow(tr("Initial path:"),   m_initialRemote);

    tabs->addTab(generalTab, tr("General"));

    // ── Tab: Authentication ───────────────────────────────────────────────────
    auto *authTab    = new QWidget;
    auto *authLayout = new QFormLayout(authTab);
    authLayout->setContentsMargins(12, 12, 12, 12);
    authLayout->setSpacing(8);

    m_authMethod = new QComboBox(authTab);
    m_authMethod->addItem(tr("Password"),   static_cast<int>(core::ssh::AuthMethod::Password));
    m_authMethod->addItem(tr("Public Key"), static_cast<int>(core::ssh::AuthMethod::PublicKey));
    m_authMethod->addItem(tr("SSH Agent"),  static_cast<int>(core::ssh::AuthMethod::Agent));

    m_password = new QLineEdit(authTab);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Leave empty — ask on connect"));

    m_keyPath  = new QLineEdit(authTab);
    m_browseKey = new QPushButton(tr("Browse…"), authTab);
    connect(m_browseKey, &QPushButton::clicked, this, &SessionDialog::onBrowseKey);

    auto *keyRow = new QHBoxLayout;
    keyRow->addWidget(m_keyPath);
    keyRow->addWidget(m_browseKey);

    m_useAgent = new QCheckBox(tr("Use system ssh-agent"), authTab);

    authLayout->addRow(tr("Method:"),     m_authMethod);
    authLayout->addRow(tr("Password:"),   m_password);
    authLayout->addRow(tr("Key file:"),   keyRow);
    authLayout->addRow(QString{},         m_useAgent);

    connect(m_authMethod, &QComboBox::currentIndexChanged,
            this, &SessionDialog::onAuthMethodChanged);
    onAuthMethodChanged(0);

    tabs->addTab(authTab, tr("Authentication"));

    // ── Buttons ───────────────────────────────────────────────────────────────
    m_testBtn    = new QPushButton(tr("Test Connection"), this);
    m_testStatus = new QLabel(this);
    connect(m_testBtn, &QPushButton::clicked, this, &SessionDialog::onTestConnection);

    auto *testRow = new QHBoxLayout;
    testRow->addWidget(m_testBtn);
    testRow->addWidget(m_testStatus, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabs);
    mainLayout->addLayout(testRow);
    mainLayout->addWidget(buttons);
}

void SessionDialog::populate(const core::session::SessionProfile &p)
{
    m_name->setText(p.name);
    m_host->setText(p.host);
    m_port->setValue(p.port);
    m_username->setText(p.username);
    m_initialRemote->setText(p.initialRemotePath);
    m_keyPath->setText(p.privateKeyPath);

    int idx = m_authMethod->findData(static_cast<int>(p.authMethod));
    if (idx >= 0) m_authMethod->setCurrentIndex(idx);
}

core::session::SessionProfile SessionDialog::profile() const
{
    core::session::SessionProfile p;
    p.name              = m_name->text().trimmed();
    p.host              = m_host->text().trimmed();
    p.port              = static_cast<quint16>(m_port->value());
    p.username          = m_username->text().trimmed();
    p.initialRemotePath = m_initialRemote->text();
    p.authMethod        = static_cast<core::ssh::AuthMethod>(
        m_authMethod->currentData().toInt());
    p.privateKeyPath    = m_keyPath->text();
    p.useAgent          = m_useAgent->isChecked();
    return p;
}

void SessionDialog::onAuthMethodChanged(int)
{
    const auto method = static_cast<core::ssh::AuthMethod>(
        m_authMethod->currentData().toInt());

    m_password->setEnabled(method == core::ssh::AuthMethod::Password);
    m_keyPath->setEnabled(method == core::ssh::AuthMethod::PublicKey);
    m_browseKey->setEnabled(method == core::ssh::AuthMethod::PublicKey);
    m_useAgent->setEnabled(method == core::ssh::AuthMethod::Agent);
}

void SessionDialog::onBrowseKey()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Private Key"),
        QDir::homePath() + "/.ssh",
        tr("Key files (id_rsa id_ed25519 id_ecdsa *.pem *.ppk);;All files (*)"));
    if (!path.isEmpty()) m_keyPath->setText(path);
}

void SessionDialog::onTestConnection()
{
    m_testStatus->setText(tr("Testing…"));
    // TODO: создать временную SshSession и проверить подключение асинхронно
    m_testStatus->setText(tr("Not implemented yet"));
}

} // namespace linscp::ui::dialogs
