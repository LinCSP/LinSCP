#include "session_dialog.h"
#include "advanced_session_dialog.h"
#include "core/webdav/webdav_client.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QDir>

namespace linscp::ui::dialogs {

SessionDialog::SessionDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
}

SessionDialog::SessionDialog(const core::session::SessionProfile &profile, QWidget *parent)
    : QDialog(parent)
{
    m_advancedData = profile;
    setupUi();
    populate(profile);
}

void SessionDialog::setupUi()
{
    setWindowTitle(tr("Login"));
    setFixedWidth(420);

    // ── Connection group ──────────────────────────────────────────────────────
    auto *connGroup  = new QGroupBox(tr("Connection"), this);
    auto *connLayout = new QFormLayout(connGroup);
    connLayout->setSpacing(8);

    m_protocol = new QComboBox(connGroup);
    m_protocol->addItem("SFTP",   static_cast<int>(core::session::TransferProtocol::Sftp));
    m_protocol->addItem("SCP",    static_cast<int>(core::session::TransferProtocol::Scp));
    m_protocol->addItem("WebDAV", static_cast<int>(core::session::TransferProtocol::WebDav));

    m_encryption = new QComboBox(connGroup);
    m_encryption->addItem(tr("No encryption"),   static_cast<int>(core::webdav::WebDavEncryption::None));
    m_encryption->addItem(tr("TLS/SSL"),          static_cast<int>(core::webdav::WebDavEncryption::Tls));
    m_encryption->addItem(tr("Implicit TLS"),     static_cast<int>(core::webdav::WebDavEncryption::ImplicitTls));
    m_encryptionLabel = new QLabel(tr("Encryption:"), connGroup);

    m_host = new QLineEdit(connGroup);
    m_host->setPlaceholderText(tr("hostname or IP"));

    m_port = new QSpinBox(connGroup);
    m_port->setRange(1, 65535);
    m_port->setValue(22);

    auto *hostRow = new QHBoxLayout;
    hostRow->setContentsMargins(0,0,0,0);
    hostRow->addWidget(m_host, 1);
    hostRow->addWidget(new QLabel(tr("Port:"), connGroup));
    hostRow->addWidget(m_port);

    connLayout->addRow(tr("Protocol:"),    m_protocol);
    connLayout->addRow(m_encryptionLabel,  m_encryption);
    connLayout->addRow(tr("Host:"),        hostRow);

    connect(m_protocol,   &QComboBox::currentIndexChanged,
            this, &SessionDialog::onProtocolChanged);
    connect(m_encryption, &QComboBox::currentIndexChanged,
            this, &SessionDialog::onEncryptionChanged);

    // ── Auth group ────────────────────────────────────────────────────────────
    auto *authGroup  = new QGroupBox(tr("Authentication"), this);
    auto *authLayout = new QFormLayout(authGroup);
    authLayout->setSpacing(8);

    m_username = new QLineEdit(authGroup);

    m_authMethod = new QComboBox(authGroup);
    m_authMethod->addItem(tr("Password"),   static_cast<int>(core::ssh::AuthMethod::Password));
    m_authMethod->addItem(tr("Public Key"), static_cast<int>(core::ssh::AuthMethod::PublicKey));
    m_authMethod->addItem(tr("SSH Agent"),  static_cast<int>(core::ssh::AuthMethod::Agent));

    m_password = new QLineEdit(authGroup);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("leave empty — prompt on connect"));

    m_keyPath   = new QLineEdit(authGroup);
    m_browseKey = new QPushButton(tr("Browse…"), authGroup);
    connect(m_browseKey, &QPushButton::clicked, this, &SessionDialog::onBrowseKey);

    auto *keyRow = new QHBoxLayout;
    keyRow->setContentsMargins(0,0,0,0);
    keyRow->addWidget(m_keyPath, 1);
    keyRow->addWidget(m_browseKey);

    m_authMethodLabel = new QLabel(tr("Method:"), authGroup);

    authLayout->addRow(tr("Username:"),  m_username);
    authLayout->addRow(m_authMethodLabel, m_authMethod);
    authLayout->addRow(tr("Password:"), m_password);
    authLayout->addRow(tr("Key file:"), keyRow);

    connect(m_authMethod, &QComboBox::currentIndexChanged,
            this, &SessionDialog::onAuthMethodChanged);
    onAuthMethodChanged(0);
    onProtocolChanged(0);  // инициализировать видимость для начального протокола

    // ── Test row ──────────────────────────────────────────────────────────────
    m_testBtn    = new QPushButton(tr("Test Connection"), this);
    m_testStatus = new QLabel(this);
    connect(m_testBtn, &QPushButton::clicked, this, &SessionDialog::onTestConnection);

    auto *testRow = new QHBoxLayout;
    testRow->addWidget(m_testBtn);
    testRow->addWidget(m_testStatus, 1);

    // ── Buttons: Save | Cancel | More… ────────────────────────────────────────
    auto *moreBtn   = new QPushButton(tr("More…"), this);
    moreBtn->setToolTip(tr("Advanced session settings"));
    connect(moreBtn, &QPushButton::clicked, this, &SessionDialog::onMoreClicked);

    auto *btnBox = new QDialogButtonBox(this);
    auto *saveBtn   = btnBox->addButton(tr("Save"),   QDialogButtonBox::AcceptRole);
    auto *cancelBtn = btnBox->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    Q_UNUSED(saveBtn)
    Q_UNUSED(cancelBtn)
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(moreBtn);
    btnRow->addStretch();
    btnRow->addWidget(btnBox);

    // ── Main layout ───────────────────────────────────────────────────────────
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(connGroup);
    mainLayout->addWidget(authGroup);
    mainLayout->addLayout(testRow);
    mainLayout->addLayout(btnRow);
}

void SessionDialog::populate(const core::session::SessionProfile &p)
{
    m_protocol->setCurrentIndex(m_protocol->findData(static_cast<int>(p.protocol)));
    m_host->setText(p.host);
    m_port->setValue(p.port);
    m_username->setText(p.username);
    m_keyPath->setText(p.privateKeyPath);

    const int idx = m_authMethod->findData(static_cast<int>(p.authMethod));
    if (idx >= 0) m_authMethod->setCurrentIndex(idx);

    const int encIdx = m_encryption->findData(static_cast<int>(p.webDavEncryption));
    if (encIdx >= 0) m_encryption->setCurrentIndex(encIdx);
}

core::session::SessionProfile SessionDialog::profile() const
{
    core::session::SessionProfile p = m_advancedData;

    p.protocol  = static_cast<core::session::TransferProtocol>(
        m_protocol->currentData().toInt());
    p.host      = m_host->text().trimmed();
    p.port      = static_cast<quint16>(m_port->value());
    p.username  = m_username->text().trimmed();
    p.authMethod = static_cast<core::ssh::AuthMethod>(
        m_authMethod->currentData().toInt());
    p.privateKeyPath = m_keyPath->text();
    p.webDavEncryption = static_cast<core::webdav::WebDavEncryption>(
        m_encryption->currentData().toInt());

    return p;
}

void SessionDialog::onAuthMethodChanged(int)
{
    const auto method = static_cast<core::ssh::AuthMethod>(
        m_authMethod->currentData().toInt());

    m_password->setVisible(method == core::ssh::AuthMethod::Password);
    m_keyPath->setVisible(method == core::ssh::AuthMethod::PublicKey);
    m_browseKey->setVisible(method == core::ssh::AuthMethod::PublicKey);
}

bool SessionDialog::isWebDav() const
{
    return static_cast<core::session::TransferProtocol>(
               m_protocol->currentData().toInt())
           == core::session::TransferProtocol::WebDav;
}

void SessionDialog::onProtocolChanged(int)
{
    const bool webdav = isWebDav();

    // Строка шифрования — только для WebDAV
    m_encryptionLabel->setVisible(webdav);
    m_encryption->setVisible(webdav);

    // Метод аутентификации — не нужен для WebDAV (всегда Password)
    if (m_authMethodLabel)
        m_authMethodLabel->setVisible(!webdav);
    m_authMethod->setVisible(!webdav);

    // Порт по умолчанию
    if (webdav) {
        const auto enc = static_cast<core::webdav::WebDavEncryption>(
            m_encryption->currentData().toInt());
        m_port->setValue(enc == core::webdav::WebDavEncryption::None ? 80 : 443);
    } else {
        m_port->setValue(22);
    }

    // Ключ/агент — недоступны при WebDAV
    if (webdav) {
        const int pwdIdx = m_authMethod->findData(
            static_cast<int>(core::ssh::AuthMethod::Password));
        if (pwdIdx >= 0) m_authMethod->setCurrentIndex(pwdIdx);
        onAuthMethodChanged(0);
    }
}

void SessionDialog::onEncryptionChanged(int)
{
    if (!isWebDav()) return;

    const auto enc = static_cast<core::webdav::WebDavEncryption>(
        m_encryption->currentData().toInt());

    // Авто-корректировка порта, только если он ещё не менялся вручную
    const int cur = m_port->value();
    const bool wasHttp  = (cur == 80);
    const bool wasHttps = (cur == 443);
    if (wasHttp || wasHttps) {
        m_port->setValue(enc == core::webdav::WebDavEncryption::None ? 80 : 443);
    }
}

void SessionDialog::onBrowseKey()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Private Key"),
        QDir::homePath() + "/.ssh",
        tr("Key files (id_rsa id_ed25519 id_ecdsa *.pem *.ppk);;All files (*)"));
    if (!path.isEmpty()) m_keyPath->setText(path);
}

void SessionDialog::onMoreClicked()
{
    // Сохраняем базовые поля в m_advancedData перед открытием диалога,
    // чтобы AdvancedSessionDialog видел актуальные пути
    m_advancedData.protocol      = static_cast<core::session::TransferProtocol>(
        m_protocol->currentData().toInt());
    m_advancedData.host          = m_host->text().trimmed();
    m_advancedData.port          = static_cast<quint16>(m_port->value());
    m_advancedData.username      = m_username->text().trimmed();
    m_advancedData.authMethod    = static_cast<core::ssh::AuthMethod>(
        m_authMethod->currentData().toInt());
    m_advancedData.privateKeyPath = m_keyPath->text();

    AdvancedSessionDialog dlg(m_advancedData, this);
    if (dlg.exec() == QDialog::Accepted)
        m_advancedData = dlg.applyTo(m_advancedData);
}

void SessionDialog::onTestConnection()
{
    m_testStatus->setText(tr("Testing…"));
    // TODO: создать временную SshSession и проверить подключение асинхронно
    m_testStatus->setText(tr("Not implemented yet"));
}

} // namespace linscp::ui::dialogs
