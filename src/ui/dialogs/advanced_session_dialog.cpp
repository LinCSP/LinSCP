#include "advanced_session_dialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QSplitter>
#include <QFrame>

namespace linscp::ui::dialogs {

// ─── helpers ─────────────────────────────────────────────────────────────────

static QWidget *makePage(QLayout *layout)
{
    auto *w = new QWidget;
    layout->setContentsMargins(16, 12, 16, 12);
    w->setLayout(layout);
    return w;
}

// ─── construction ─────────────────────────────────────────────────────────────

AdvancedSessionDialog::AdvancedSessionDialog(const core::session::SessionProfile &profile,
                                             QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    populate(profile);
}

void AdvancedSessionDialog::setupUi()
{
    setWindowTitle(tr("Advanced Session Settings"));
    resize(720, 520);

    // ── Layout: tree | stack ──────────────────────────────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    m_tree = new QTreeWidget(splitter);
    m_tree->setHeaderHidden(true);
    m_tree->setFixedWidth(180);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(16);

    m_stack = new QStackedWidget(splitter);
    splitter->addWidget(m_tree);
    splitter->addWidget(m_stack);
    splitter->setStretchFactor(1, 1);

    // ── Tree items ────────────────────────────────────────────────────────────
    auto addRoot = [&](const QString &text) -> QTreeWidgetItem * {
        auto *item = new QTreeWidgetItem(m_tree, QStringList{text});
        item->setExpanded(true);
        return item;
    };
    auto addChild = [&](QTreeWidgetItem *parent, const QString &text) -> QTreeWidgetItem * {
        return new QTreeWidgetItem(parent, QStringList{text});
    };

    auto *envRoot  = addRoot(tr("Environment"));
    auto *dirItem  = addChild(envRoot, tr("Directories"));
    auto *binItem  = addChild(envRoot, tr("Recycle Bin"));
    auto *scpItem  = addChild(envRoot, tr("SCP/Shell"));
    auto *envItem  = envRoot; // «Среда» корневой — показывает общие настройки среды

    auto *connRoot = addRoot(tr("Connection"));
    auto *proxyItem  = addChild(connRoot, tr("Proxy"));
    auto *tunnelItem = addChild(connRoot, tr("Tunnel"));

    auto *sshRoot  = addRoot(tr("SSH"));
    auto *sshAuth  = addChild(sshRoot, tr("Authentication"));

    auto *noteItem = new QTreeWidgetItem(m_tree, QStringList{tr("Note")});

    // Индексы страниц совпадают с порядком addWidget
    // 0 — Среда (общая)
    // 1 — Каталоги
    // 2 — Корзина
    // 3 — SCP/Оболочка
    // 4 — Прокси
    // 5 — Туннель
    // 6 — SSH/Аутентификация
    // 7 — Заметка

    envItem->setData(0, Qt::UserRole, 0);
    dirItem->setData(0, Qt::UserRole, 1);
    binItem->setData(0, Qt::UserRole, 2);
    scpItem->setData(0, Qt::UserRole, 3);
    proxyItem->setData(0, Qt::UserRole, 4);
    tunnelItem->setData(0, Qt::UserRole, 5);
    sshAuth->setData(0, Qt::UserRole, 6);
    sshRoot->setData(0, Qt::UserRole, 6);
    noteItem->setData(0, Qt::UserRole, 7);

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, &AdvancedSessionDialog::onCategoryChanged);

    // ── Page 0: Среда (общая) ─────────────────────────────────────────────────
    {
        auto *form = new QFormLayout;
        m_dstMode = new QComboBox;
        m_dstMode->addItem(tr("Adjust to server (Unix)"),    static_cast<int>(core::session::DstMode::Unix));
        m_dstMode->addItem(tr("Adjust to server (Windows)"), static_cast<int>(core::session::DstMode::Windows));
        m_dstMode->addItem(tr("Keep server time"),           static_cast<int>(core::session::DstMode::Keep));
        m_utf8FileNames = new QCheckBox(tr("UTF-8 encoding for file names"));
        m_timezoneOffset = new QSpinBox;
        m_timezoneOffset->setRange(-720, 720);
        m_timezoneOffset->setSuffix(tr(" min"));

        auto *group = new QGroupBox(tr("Server environment"));
        auto *gl = new QFormLayout(group);
        gl->addRow(tr("Daylight saving time:"), m_dstMode);
        gl->addRow(tr("Timezone offset:"), m_timezoneOffset);
        gl->addRow(m_utf8FileNames);

        form->addRow(group);
        m_stack->addWidget(makePage(form));
    }

    // ── Page 1: Каталоги ──────────────────────────────────────────────────────
    {
        auto *form = new QFormLayout;
        m_remoteDir     = new QLineEdit;
        m_remoteDir->setPlaceholderText(tr("/ (default)"));
        m_localDir      = new QLineEdit;
        m_localDir->setPlaceholderText(QDir::homePath());
        m_syncBrowsing  = new QCheckBox(tr("Synchronize browsing (local follows remote)"));

        form->addRow(tr("Remote directory:"), m_remoteDir);
        form->addRow(tr("Local directory:"),  m_localDir);
        form->addRow(m_syncBrowsing);
        m_stack->addWidget(makePage(form));
    }

    // ── Page 2: Корзина ───────────────────────────────────────────────────────
    {
        auto *form = new QFormLayout;
        m_deleteToRecycleBin = new QCheckBox(tr("Move deleted files to recycle bin"));
        m_recycleBinPath     = new QLineEdit;
        m_recycleBinPath->setPlaceholderText(tr("/tmp/.trash (example)"));

        form->addRow(m_deleteToRecycleBin);
        form->addRow(tr("Recycle bin path:"), m_recycleBinPath);

        connect(m_deleteToRecycleBin, &QCheckBox::toggled,
                m_recycleBinPath, &QWidget::setEnabled);
        m_recycleBinPath->setEnabled(false);

        m_stack->addWidget(makePage(form));
    }

    // ── Page 3: SCP/Оболочка ─────────────────────────────────────────────────
    {
        auto *form = new QFormLayout;
        m_shell = new QLineEdit;
        m_shell->setPlaceholderText(tr("default shell"));
        m_listingCommand = new QLineEdit;
        m_listingCommand->setPlaceholderText("ls -la");
        m_clearAliases      = new QCheckBox(tr("Clear aliases"));
        m_ignoreLsWarnings  = new QCheckBox(tr("Ignore ls warnings"));
        m_eolType = new QComboBox;
        m_eolType->addItem("LF (Unix)",   static_cast<int>(core::session::EolType::LF));
        m_eolType->addItem("CRLF (Windows)", static_cast<int>(core::session::EolType::CRLF));
        m_eolType->addItem("CR (Mac)",    static_cast<int>(core::session::EolType::CR));

        form->addRow(tr("Shell:"),            m_shell);
        form->addRow(tr("Listing command:"),  m_listingCommand);
        form->addRow(tr("EOL type:"),         m_eolType);
        form->addRow(m_clearAliases);
        form->addRow(m_ignoreLsWarnings);
        m_stack->addWidget(makePage(form));
    }

    // ── Page 4: Прокси ────────────────────────────────────────────────────────
    {
        auto *form = new QFormLayout;
        m_proxyMethod = new QComboBox;
        m_proxyMethod->addItem(tr("None"),        static_cast<int>(core::session::ProxyMethod::None));
        m_proxyMethod->addItem("SOCKS4",          static_cast<int>(core::session::ProxyMethod::Socks4));
        m_proxyMethod->addItem("SOCKS5",          static_cast<int>(core::session::ProxyMethod::Socks5));
        m_proxyMethod->addItem("HTTP",            static_cast<int>(core::session::ProxyMethod::Http));
        m_proxyMethod->addItem("Telnet",          static_cast<int>(core::session::ProxyMethod::Telnet));
        m_proxyMethod->addItem(tr("Local cmd"),   static_cast<int>(core::session::ProxyMethod::LocalCmd));

        m_proxyHost    = new QLineEdit;
        m_proxyPort    = new QSpinBox;
        m_proxyPort->setRange(0, 65535);
        m_proxyUser    = new QLineEdit;
        m_proxyCommand = new QLineEdit;
        m_proxyCommand->setPlaceholderText(tr("Command / Telnet script"));
        m_proxyDns     = new QCheckBox(tr("Resolve DNS through proxy"));

        form->addRow(tr("Method:"),   m_proxyMethod);
        form->addRow(tr("Host:"),     m_proxyHost);
        form->addRow(tr("Port:"),     m_proxyPort);
        form->addRow(tr("Username:"), m_proxyUser);
        form->addRow(tr("Command:"),  m_proxyCommand);
        form->addRow(m_proxyDns);

        auto updateProxy = [=](int) {
            const auto m = static_cast<core::session::ProxyMethod>(
                m_proxyMethod->currentData().toInt());
            const bool hasHost = (m != core::session::ProxyMethod::None
                                  && m != core::session::ProxyMethod::LocalCmd);
            m_proxyHost->setEnabled(hasHost);
            m_proxyPort->setEnabled(hasHost);
            m_proxyUser->setEnabled(hasHost);
            m_proxyCommand->setEnabled(m == core::session::ProxyMethod::Telnet
                                       || m == core::session::ProxyMethod::LocalCmd);
            m_proxyDns->setEnabled(hasHost);
        };
        connect(m_proxyMethod, &QComboBox::currentIndexChanged, updateProxy);
        updateProxy(0);

        m_stack->addWidget(makePage(form));
    }

    // ── Page 5: Туннель ───────────────────────────────────────────────────────
    {
        auto *form = new QFormLayout;
        m_tunnelEnabled = new QCheckBox(tr("Connect through SSH tunnel (jump host)"));
        m_tunnelHost    = new QLineEdit;
        m_tunnelPort    = new QSpinBox;
        m_tunnelPort->setRange(1, 65535);
        m_tunnelPort->setValue(22);
        m_tunnelUser    = new QLineEdit;
        m_tunnelAuth    = new QComboBox;
        m_tunnelAuth->addItem(tr("Password"), static_cast<int>(core::ssh::AuthMethod::Password));
        m_tunnelAuth->addItem(tr("Key file"), static_cast<int>(core::ssh::AuthMethod::PublicKey));
        m_tunnelAuth->addItem(tr("Agent"),    static_cast<int>(core::ssh::AuthMethod::Agent));
        m_tunnelKeyPath   = new QLineEdit;
        m_tunnelBrowseKey = new QPushButton(tr("Browse…"));

        auto *keyRow = new QHBoxLayout;
        keyRow->setContentsMargins(0,0,0,0);
        auto *keyWrap = new QWidget;
        keyWrap->setLayout(keyRow);
        keyRow->addWidget(m_tunnelKeyPath);
        keyRow->addWidget(m_tunnelBrowseKey);

        form->addRow(m_tunnelEnabled);
        form->addRow(tr("Host:"),     m_tunnelHost);
        form->addRow(tr("Port:"),     m_tunnelPort);
        form->addRow(tr("Username:"), m_tunnelUser);
        form->addRow(tr("Auth:"),     m_tunnelAuth);
        form->addRow(tr("Key file:"), keyWrap);

        auto updateTunnel = [=](bool on) {
            m_tunnelHost->setEnabled(on);
            m_tunnelPort->setEnabled(on);
            m_tunnelUser->setEnabled(on);
            m_tunnelAuth->setEnabled(on);
            m_tunnelKeyPath->setEnabled(on);
            m_tunnelBrowseKey->setEnabled(on);
        };
        connect(m_tunnelEnabled, &QCheckBox::toggled, updateTunnel);
        updateTunnel(false);

        connect(m_tunnelBrowseKey, &QPushButton::clicked, this, [=] {
            const QString path = QFileDialog::getOpenFileName(
                this, tr("Select Private Key"),
                QDir::homePath() + "/.ssh",
                tr("Key files (id_rsa id_ed25519 *.pem *.ppk);;All files (*)"));
            if (!path.isEmpty()) m_tunnelKeyPath->setText(path);
        });

        connect(m_tunnelAuth, &QComboBox::currentIndexChanged, this, [=](int) {
            const bool isKey = static_cast<core::ssh::AuthMethod>(
                m_tunnelAuth->currentData().toInt()) == core::ssh::AuthMethod::PublicKey;
            m_tunnelKeyPath->setVisible(isKey);
            m_tunnelBrowseKey->setVisible(isKey);
        });

        m_stack->addWidget(makePage(form));
    }

    // ── Page 6: SSH / Аутентификация ─────────────────────────────────────────
    {
        auto *form = new QFormLayout;
        m_compression    = new QCheckBox(tr("Enable SSH compression"));
        m_keepalive      = new QSpinBox;
        m_keepalive->setRange(0, 3600);
        m_keepalive->setSuffix(tr(" s (0 = off)"));
        m_connectTimeout = new QSpinBox;
        m_connectTimeout->setRange(5, 300);
        m_connectTimeout->setSuffix(tr(" s"));
        m_connectTimeout->setValue(15);

        form->addRow(m_compression);
        form->addRow(tr("Keepalive interval:"), m_keepalive);
        form->addRow(tr("Connect timeout:"),    m_connectTimeout);
        m_stack->addWidget(makePage(form));
    }

    // ── Page 7: Заметка ───────────────────────────────────────────────────────
    {
        auto *vbox = new QVBoxLayout;
        auto *note = new QPlainTextEdit;
        note->setObjectName("noteEdit");
        note->setPlaceholderText(tr("Notes about this session…"));
        vbox->addWidget(new QLabel(tr("Note:")));
        vbox->addWidget(note);
        m_stack->addWidget(makePage(vbox));
    }

    // ── Bottom buttons ────────────────────────────────────────────────────────
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addWidget(buttons);

    m_tree->setCurrentItem(m_tree->topLevelItem(0)->child(0)); // Каталоги
}

void AdvancedSessionDialog::onCategoryChanged(QTreeWidgetItem *current, QTreeWidgetItem *)
{
    if (!current) return;
    const int idx = current->data(0, Qt::UserRole).toInt();
    m_stack->setCurrentIndex(idx);
}

// ─── populate ────────────────────────────────────────────────────────────────

void AdvancedSessionDialog::populate(const core::session::SessionProfile &p)
{
    const auto &env = p.environment;
    const auto &prx = p.proxy;
    const auto &tun = p.tunnel;
    const auto &ssh = p.ssh;

    // Среда (общая)
    m_dstMode->setCurrentIndex(m_dstMode->findData(static_cast<int>(env.dstMode)));
    m_utf8FileNames->setChecked(env.utf8FileNames);
    m_timezoneOffset->setValue(env.timezoneOffsetMin);

    // Каталоги
    m_remoteDir->setText(p.initialRemotePath == "/" ? QString{} : p.initialRemotePath);
    m_localDir->setText(p.initialLocalPath);
    m_syncBrowsing->setChecked(p.syncBrowsing);

    // Корзина
    m_deleteToRecycleBin->setChecked(env.deleteToRecycleBin);
    m_recycleBinPath->setText(env.recycleBinPath);
    m_recycleBinPath->setEnabled(env.deleteToRecycleBin);

    // SCP/Оболочка
    m_shell->setText(env.shell);
    m_listingCommand->setText(env.listingCommand);
    m_clearAliases->setChecked(env.clearAliases);
    m_ignoreLsWarnings->setChecked(env.ignoreLsWarnings);
    m_eolType->setCurrentIndex(m_eolType->findData(static_cast<int>(env.eolType)));

    // Прокси
    m_proxyMethod->setCurrentIndex(m_proxyMethod->findData(static_cast<int>(prx.method)));
    m_proxyHost->setText(prx.host);
    m_proxyPort->setValue(prx.port);
    m_proxyUser->setText(prx.username);
    m_proxyCommand->setText(prx.command);
    m_proxyDns->setChecked(prx.dnsViaProxy);

    // Туннель
    m_tunnelEnabled->setChecked(tun.enabled);
    m_tunnelHost->setText(tun.host);
    m_tunnelPort->setValue(tun.port ? tun.port : 22);
    m_tunnelUser->setText(tun.username);
    m_tunnelAuth->setCurrentIndex(m_tunnelAuth->findData(static_cast<int>(tun.authMethod)));
    m_tunnelKeyPath->setText(tun.keyPath);

    // SSH
    m_compression->setChecked(ssh.compression);
    m_keepalive->setValue(ssh.keepaliveInterval);
    m_connectTimeout->setValue(ssh.connectTimeout);

    // Заметка
    if (auto *note = m_stack->widget(7)->findChild<QPlainTextEdit *>("noteEdit"))
        note->setPlainText(p.notes);
}

// ─── applyTo ─────────────────────────────────────────────────────────────────

core::session::SessionProfile
AdvancedSessionDialog::applyTo(core::session::SessionProfile base) const
{
    auto &env = base.environment;
    auto &prx = base.proxy;
    auto &tun = base.tunnel;
    auto &ssh = base.ssh;

    // Среда (общая)
    env.dstMode          = static_cast<core::session::DstMode>(m_dstMode->currentData().toInt());
    env.utf8FileNames    = m_utf8FileNames->isChecked();
    env.timezoneOffsetMin = m_timezoneOffset->value();

    // Каталоги
    base.initialRemotePath = m_remoteDir->text().isEmpty() ? QStringLiteral("/") : m_remoteDir->text();
    base.initialLocalPath  = m_localDir->text();
    base.syncBrowsing      = m_syncBrowsing->isChecked();

    // Корзина
    env.deleteToRecycleBin = m_deleteToRecycleBin->isChecked();
    env.recycleBinPath     = m_recycleBinPath->text();

    // SCP/Оболочка
    env.shell             = m_shell->text();
    env.listingCommand    = m_listingCommand->text();
    env.clearAliases      = m_clearAliases->isChecked();
    env.ignoreLsWarnings  = m_ignoreLsWarnings->isChecked();
    env.eolType           = static_cast<core::session::EolType>(m_eolType->currentData().toInt());

    // Прокси
    prx.method     = static_cast<core::session::ProxyMethod>(m_proxyMethod->currentData().toInt());
    prx.host       = m_proxyHost->text();
    prx.port       = static_cast<quint16>(m_proxyPort->value());
    prx.username   = m_proxyUser->text();
    prx.command    = m_proxyCommand->text();
    prx.dnsViaProxy = m_proxyDns->isChecked();

    // Туннель
    tun.enabled    = m_tunnelEnabled->isChecked();
    tun.host       = m_tunnelHost->text();
    tun.port       = static_cast<quint16>(m_tunnelPort->value());
    tun.username   = m_tunnelUser->text();
    tun.authMethod = static_cast<core::ssh::AuthMethod>(m_tunnelAuth->currentData().toInt());
    tun.keyPath    = m_tunnelKeyPath->text();

    // SSH
    ssh.compression       = m_compression->isChecked();
    ssh.keepaliveInterval = m_keepalive->value();
    ssh.connectTimeout    = m_connectTimeout->value();

    // Заметка
    if (auto *note = m_stack->widget(7)->findChild<QPlainTextEdit *>("noteEdit"))
        base.notes = note->toPlainText();

    return base;
}

} // namespace linscp::ui::dialogs
