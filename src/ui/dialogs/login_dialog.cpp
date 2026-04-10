#include "login_dialog.h"
#include "advanced_session_dialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStackedWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QDir>
#include <QUuid>

namespace linscp::ui::dialogs {

// ─── Tree item user-data roles ────────────────────────────────────────────────

static constexpr int RoleType    = Qt::UserRole;       // "folder" | "session" | "new"
static constexpr int RoleUuid    = Qt::UserRole + 1;   // QUuid (sessions only)

// ─── construction ─────────────────────────────────────────────────────────────

LoginDialog::LoginDialog(core::session::SessionStore *store, QWidget *parent)
    : QDialog(parent), m_store(store)
{
    setupUi();
    buildTree();

    // Выбрать "Новое подключение" по умолчанию
    if (m_tree->topLevelItemCount() > 0)
        m_tree->setCurrentItem(m_tree->topLevelItem(0));
}

// ─── setupUi ─────────────────────────────────────────────────────────────────

void LoginDialog::setupUi()
{
    setWindowTitle(tr("Login"));
    resize(760, 520);
    setMinimumSize(600, 400);

    // ── Left: session tree ────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setMinimumWidth(230);
    m_tree->setMaximumWidth(320);
    m_tree->setIndentation(16);
    m_tree->setRootIsDecorated(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *, QTreeWidgetItem *) {
                onTreeSelectionChanged();
            });
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *item, int) {
                if (item && item->data(0, RoleType).toString() == "session")
                    onLogin();
            });

    // ── Right: connection form ────────────────────────────────────────────────
    m_connGroup  = new QGroupBox(tr("Connection"), this);
    auto *formLayout = new QFormLayout(m_connGroup);
    formLayout->setSpacing(8);
    formLayout->setContentsMargins(12, 12, 12, 12);

    m_protocol = new QComboBox(m_connGroup);
    m_protocol->addItem("SFTP", static_cast<int>(core::session::TransferProtocol::Sftp));
    m_protocol->addItem("SCP",  static_cast<int>(core::session::TransferProtocol::Scp));

    m_host = new QLineEdit(m_connGroup);
    m_host->setPlaceholderText(tr("hostname or IP"));

    m_port = new QSpinBox(m_connGroup);
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    m_port->setFixedWidth(80);

    auto *hostRow = new QHBoxLayout;
    hostRow->setContentsMargins(0,0,0,0);
    hostRow->addWidget(m_host, 1);
    hostRow->addWidget(new QLabel(tr("Port:"), m_connGroup));
    hostRow->addWidget(m_port);

    m_username = new QLineEdit(m_connGroup);

    m_authMethod = new QComboBox(m_connGroup);
    m_authMethod->addItem(tr("Password"),   static_cast<int>(core::ssh::AuthMethod::Password));
    m_authMethod->addItem(tr("Public Key"), static_cast<int>(core::ssh::AuthMethod::PublicKey));
    m_authMethod->addItem(tr("SSH Agent"),  static_cast<int>(core::ssh::AuthMethod::Agent));

    m_password = new QLineEdit(m_connGroup);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("leave empty — prompt on connect"));

    m_keyPath   = new QLineEdit(m_connGroup);
    m_browseKey = new QPushButton(tr("Browse…"), m_connGroup);
    connect(m_browseKey, &QPushButton::clicked, this, &LoginDialog::onBrowseKey);

    auto *keyRow = new QHBoxLayout;
    keyRow->setContentsMargins(0,0,0,0);
    keyRow->addWidget(m_keyPath, 1);
    keyRow->addWidget(m_browseKey);

    formLayout->addRow(tr("Protocol:"),   m_protocol);
    formLayout->addRow(tr("Hostname:"),   hostRow);
    formLayout->addRow(tr("Username:"),   m_username);
    formLayout->addRow(tr("Auth:"),       m_authMethod);
    formLayout->addRow(tr("Password:"),   m_password);
    formLayout->addRow(tr("Key file:"),   keyRow);

    connect(m_authMethod, &QComboBox::currentIndexChanged,
            this, &LoginDialog::onAuthMethodChanged);
    onAuthMethodChanged(0);

    // Кнопки под формой: [Сохранить] [Ещё… ▼]
    m_saveBtn = new QPushButton(tr("Save"), m_connGroup);
    connect(m_saveBtn, &QPushButton::clicked, this, &LoginDialog::onSave);

    m_advancedBtn = new QPushButton(tr("Advanced…"), m_connGroup);
    connect(m_advancedBtn, &QPushButton::clicked, this, &LoginDialog::onAdvanced);

    auto *saveBtnRow = new QHBoxLayout;
    saveBtnRow->setContentsMargins(0, 4, 0, 0);
    saveBtnRow->addWidget(m_saveBtn);
    saveBtnRow->addWidget(m_advancedBtn);
    saveBtnRow->addStretch();
    formLayout->addRow(saveBtnRow);

    // ── Note group ────────────────────────────────────────────────────────────
    m_noteGroup = new QGroupBox(tr("Note"), this);
    auto *noteLayout = new QVBoxLayout(m_noteGroup);
    noteLayout->setContentsMargins(8, 8, 8, 8);
    m_noteMemo = new QPlainTextEdit(m_noteGroup);
    m_noteMemo->setMaximumHeight(80);
    m_noteMemo->setPlaceholderText(tr("Session notes…"));
    noteLayout->addWidget(m_noteMemo);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(m_connGroup);
    rightLayout->addWidget(m_noteGroup);
    rightLayout->addStretch();

    auto *rightWidget = new QWidget(this);
    rightWidget->setLayout(rightLayout);

    // ── Splitter: tree | form ─────────────────────────────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(m_tree);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    // ── Bottom button row ─────────────────────────────────────────────────────

    // Инструменты ▼
    m_toolsMenu = new QMenu(this);
    m_toolsMenu->addAction(tr("Import sessions…"), this, &LoginDialog::onImportSessions);
    m_toolsMenu->addAction(tr("Export sessions…"), this, &LoginDialog::onExportSessions);

    m_toolsBtn = new QToolButton(this);
    m_toolsBtn->setText(tr("Tools"));
    m_toolsBtn->setPopupMode(QToolButton::MenuButtonPopup);
    m_toolsBtn->setMenu(m_toolsMenu);
    connect(m_toolsBtn, &QToolButton::clicked, m_toolsBtn, &QToolButton::showMenu);

    // Управление ▼
    m_manageMenu   = new QMenu(this);
    m_actRename    = m_manageMenu->addAction(tr("Rename"),    this, &LoginDialog::onRenameSession);
    m_actDelete    = m_manageMenu->addAction(tr("Delete"),    this, &LoginDialog::onDeleteSession);
    m_actDuplicate = m_manageMenu->addAction(tr("Duplicate"), this, &LoginDialog::onDuplicateSession);
    m_manageMenu->addSeparator();
    m_manageMenu->addAction(tr("New folder"), this, &LoginDialog::onNewFolder);

    m_manageBtn = new QToolButton(this);
    m_manageBtn->setText(tr("Manage"));
    m_manageBtn->setPopupMode(QToolButton::MenuButtonPopup);
    m_manageBtn->setMenu(m_manageMenu);
    connect(m_manageBtn, &QToolButton::clicked, m_manageBtn, &QToolButton::showMenu);

    // Войти
    m_loginBtn = new QPushButton(tr("Login"), this);
    m_loginBtn->setDefault(true);
    m_loginBtn->setFixedWidth(90);
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);

    // Закрыть
    m_closeBtn = new QPushButton(tr("Close"), this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addWidget(m_toolsBtn);
    bottomRow->addWidget(m_manageBtn);
    bottomRow->addStretch();
    bottomRow->addWidget(m_loginBtn);
    bottomRow->addWidget(m_closeBtn);

    // ── Main layout ───────────────────────────────────────────────────────────
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addLayout(bottomRow);

    updateButtonStates();
}

// ─── buildTree ───────────────────────────────────────────────────────────────

void LoginDialog::buildTree()
{
    m_tree->clear();

    // "Новое подключение"
    auto *newItem = new QTreeWidgetItem(m_tree, QStringList{tr("New connection")});
    newItem->setData(0, RoleType, QStringLiteral("new"));
    newItem->setIcon(0, QIcon::fromTheme("list-add"));

    // Папки из хранилища (явно сохранённые — сохраняются даже пустыми)
    for (const QString &folderPath : m_store->folders())
        ensureFolder(folderPath);

    // Сессии из хранилища
    for (const auto &profile : m_store->all()) {
        QTreeWidgetItem *parent = ensureFolder(profile.groupPath);
        auto *sessionItem = parent
            ? new QTreeWidgetItem(parent, QStringList{profile.name})
            : new QTreeWidgetItem(m_tree, QStringList{profile.name});

        sessionItem->setData(0, RoleType, QStringLiteral("session"));
        sessionItem->setData(0, RoleUuid, profile.id.toString());
        sessionItem->setIcon(0, QIcon::fromTheme("network-server"));
    }

    m_tree->expandAll();
}

QTreeWidgetItem *LoginDialog::currentFolderItem() const
{
    auto *item = m_tree->currentItem();
    if (!item) return nullptr;
    const QString type = item->data(0, RoleType).toString();
    if (type == "folder") return item;
    // Сессия или "new" — смотрим на родителя
    auto *p = item->parent();
    if (p && p->data(0, RoleType).toString() == "folder") return p;
    return nullptr;
}

QTreeWidgetItem *LoginDialog::ensureFolder(const QString &groupPath)
{
    if (groupPath.isEmpty()) return nullptr;

    const QStringList parts = groupPath.split('/', Qt::SkipEmptyParts);
    QTreeWidgetItem *current = nullptr;
    QString builtPath;

    for (const QString &part : parts) {
        builtPath = builtPath.isEmpty() ? part : builtPath + '/' + part;

        // Ищем существующий узел с этим путём
        QTreeWidgetItem *found = nullptr;
        const int topCount = current ? current->childCount()
                                     : m_tree->topLevelItemCount();
        for (int i = 0; i < topCount; ++i) {
            QTreeWidgetItem *child = current ? current->child(i)
                                             : m_tree->topLevelItem(i);
            if (child->data(0, RoleType).toString() == "folder"
                && child->data(0, Qt::UserRole + 2).toString() == builtPath)
            {
                found = child;
                break;
            }
        }

        if (!found) {
            found = current
                ? new QTreeWidgetItem(current, QStringList{part})
                : new QTreeWidgetItem(m_tree, QStringList{part});
            found->setData(0, RoleType, QStringLiteral("folder"));
            found->setData(0, Qt::UserRole + 2, builtPath);
            found->setIcon(0, QIcon::fromTheme("folder"));
            found->setExpanded(true);
        }
        current = found;
    }
    return current;
}

// ─── Tree selection ───────────────────────────────────────────────────────────

void LoginDialog::onTreeSelectionChanged()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item) return;

    const QString type = item->data(0, RoleType).toString();

    if (type == "new") {
        m_isNewConnection = true;
        showNewConnectionForm();
    } else if (type == "session") {
        m_isNewConnection = false;
        const QUuid id = QUuid::fromString(item->data(0, RoleUuid).toString());
        showSessionForm(m_store->find(id));
    } else if (type == "folder") {
        // Папка выбрана — показываем форму нового подключения
        // (сессия будет сохранена внутри этой папки через currentFolderItem())
        m_isNewConnection = true;
        showNewConnectionForm();
    }
    updateButtonStates();
}

// ─── Forms ───────────────────────────────────────────────────────────────────

void LoginDialog::showNewConnectionForm()
{
    m_protocol->setCurrentIndex(0);
    m_host->clear();
    m_port->setValue(22);
    m_username->clear();
    m_password->clear();
    m_keyPath->clear();
    m_noteMemo->clear();
    m_authMethod->setCurrentIndex(0);
    m_selectedProfile = core::session::SessionProfile{};
    m_saveBtn->setText(tr("Save"));
    onAuthMethodChanged(0);
}

void LoginDialog::showSessionForm(const core::session::SessionProfile &p)
{
    m_selectedProfile = p;

    m_protocol->setCurrentIndex(m_protocol->findData(static_cast<int>(p.protocol)));
    m_host->setText(p.host);
    m_port->setValue(p.port);
    m_username->setText(p.username);
    m_password->clear();  // пароль не показываем в открытом виде
    m_keyPath->setText(p.privateKeyPath);
    m_noteMemo->setPlainText(p.notes);

    const int idx = m_authMethod->findData(static_cast<int>(p.authMethod));
    if (idx >= 0) m_authMethod->setCurrentIndex(idx);

    m_saveBtn->setText(tr("Save changes"));
}

core::session::SessionProfile LoginDialog::collectProfile() const
{
    core::session::SessionProfile p = m_selectedProfile;

    p.protocol  = static_cast<core::session::TransferProtocol>(
        m_protocol->currentData().toInt());
    p.host      = m_host->text().trimmed();
    p.port      = static_cast<quint16>(m_port->value());
    p.username  = m_username->text().trimmed();
    p.authMethod = static_cast<core::ssh::AuthMethod>(
        m_authMethod->currentData().toInt());
    p.privateKeyPath = m_keyPath->text();
    p.password   = m_password->text();   // транзиентно — не сохраняется на диск
    p.notes      = m_noteMemo->toPlainText();

    return p;
}

// ─── Button actions ───────────────────────────────────────────────────────────

void LoginDialog::onLogin()
{
    auto p = collectProfile();
    if (!p.isValid()) {
        QMessageBox::warning(this, tr("Login"),
                             tr("Please enter a hostname and username."));
        return;
    }
    m_selectedProfile = p;
    accept();
}

void LoginDialog::onSave()
{
    auto p = collectProfile();
    if (!p.isValid()) {
        QMessageBox::warning(this, tr("Save"), tr("Hostname and username are required."));
        return;
    }

    if (m_isNewConnection) {
        // Спросить имя если не задано
        if (p.name.isEmpty()) {
            bool ok = false;
            p.name = QInputDialog::getText(this, tr("Save session"),
                                           tr("Session name:"), QLineEdit::Normal,
                                           p.host, &ok);
            if (!ok || p.name.isEmpty()) return;
        }
        // Сохраняем в текущую папку дерева
        if (auto *folder = currentFolderItem())
            p.groupPath = folder->data(0, Qt::UserRole + 2).toString();
        m_store->add(p);
    } else {
        p.id = m_selectedProfile.id;
        m_store->update(p);
    }
    m_store->save();
    m_selectedProfile = p;
    buildTree();

    // Переселектировать только что сохранённую сессию
    // (простой обход дерева по UUID)
    const QString uuidStr = p.id.toString();
    std::function<QTreeWidgetItem*(QTreeWidgetItem*)> findItem;
    findItem = [&](QTreeWidgetItem *parent) -> QTreeWidgetItem * {
        const int count = parent ? parent->childCount()
                                 : m_tree->topLevelItemCount();
        for (int i = 0; i < count; ++i) {
            auto *child = parent ? parent->child(i) : m_tree->topLevelItem(i);
            if (child->data(0, RoleUuid).toString() == uuidStr) return child;
            if (auto *found = findItem(child)) return found;
        }
        return nullptr;
    };
    if (auto *found = findItem(nullptr))
        m_tree->setCurrentItem(found);
}

void LoginDialog::onAdvanced()
{
    auto p = collectProfile();
    AdvancedSessionDialog dlg(p, this);
    if (dlg.exec() == QDialog::Accepted)
        m_selectedProfile = dlg.applyTo(p);
}

void LoginDialog::onAuthMethodChanged(int)
{
    const auto method = static_cast<core::ssh::AuthMethod>(
        m_authMethod->currentData().toInt());

    m_password->setVisible(method == core::ssh::AuthMethod::Password);
    m_keyPath->setVisible(method == core::ssh::AuthMethod::PublicKey);
    m_browseKey->setVisible(method == core::ssh::AuthMethod::PublicKey);
}

void LoginDialog::onBrowseKey()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Private Key"),
        QDir::homePath() + "/.ssh",
        tr("Key files (id_rsa id_ed25519 id_ecdsa *.pem *.ppk);;All files (*)"));
    if (!path.isEmpty()) m_keyPath->setText(path);
}

void LoginDialog::updateButtonStates()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    const QString type = item ? item->data(0, RoleType).toString() : QString{};

    const bool isSession = (type == "session");
    // Папка тоже разрешает сохранение — новая сессия попадёт внутрь
    const bool isNewOrSession = (type == "new" || type == "session" || type == "folder");

    m_loginBtn->setEnabled(type == "new" || type == "session");
    m_saveBtn->setEnabled(isNewOrSession);
    m_advancedBtn->setEnabled(isNewOrSession);

    m_actDelete->setEnabled(isSession);
    m_actRename->setEnabled(isSession);
    m_actDuplicate->setEnabled(isSession);
}

// ─── Manage menu actions ──────────────────────────────────────────────────────

void LoginDialog::onManageMenu()
{
    m_manageBtn->showMenu();
}

void LoginDialog::onToolsMenu()
{
    m_toolsBtn->showMenu();
}

void LoginDialog::onNewFolder()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("New folder"), tr("Folder name:"),
        QLineEdit::Normal, QString{}, &ok);
    if (!ok || name.isEmpty()) return;

    // Определяем родительскую папку из текущего выбора
    QTreeWidgetItem *parentFolder = currentFolderItem();

    const QString parentPath = parentFolder
        ? parentFolder->data(0, Qt::UserRole + 2).toString()
        : QString{};
    const QString newPath = parentPath.isEmpty() ? name : parentPath + '/' + name;

    auto *folderItem = parentFolder
        ? new QTreeWidgetItem(parentFolder, QStringList{name})
        : new QTreeWidgetItem(m_tree,       QStringList{name});
    folderItem->setData(0, RoleType,        QStringLiteral("folder"));
    folderItem->setData(0, Qt::UserRole + 2, newPath);
    folderItem->setIcon(0, QIcon::fromTheme("folder"));
    folderItem->setExpanded(true);
    if (parentFolder) parentFolder->setExpanded(true);
    m_tree->setCurrentItem(folderItem);

    // Сохраняем папку в store — она будет персистентной
    m_store->addFolder(newPath);
    m_store->save();
}

void LoginDialog::onDeleteSession()
{
    auto *item = m_tree->currentItem();
    if (!item || item->data(0, RoleType).toString() != "session") return;

    const QUuid id = QUuid::fromString(item->data(0, RoleUuid).toString());
    const auto profile = m_store->find(id);

    if (QMessageBox::question(
            this, tr("Delete session"),
            tr("Delete session \"%1\"?").arg(profile.name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    m_store->remove(id);
    m_store->save();

    // Удаляем только сам узел — папки остаются (они хранятся явно в store)
    QTreeWidgetItem *parent = item->parent();
    delete item;
    Q_UNUSED(parent);

    // Выбрать "Новое подключение"
    m_tree->setCurrentItem(m_tree->topLevelItem(0));
}

void LoginDialog::onRenameSession()
{
    auto *item = m_tree->currentItem();
    if (!item || item->data(0, RoleType).toString() != "session") return;

    const QUuid id = QUuid::fromString(item->data(0, RoleUuid).toString());
    auto profile = m_store->find(id);

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this, tr("Rename session"), tr("New name:"),
        QLineEdit::Normal, profile.name, &ok);
    if (!ok || newName.isEmpty() || newName == profile.name) return;

    profile.name = newName;
    m_store->update(profile);
    m_store->save();
    item->setText(0, newName);
}

void LoginDialog::onDuplicateSession()
{
    auto *item = m_tree->currentItem();
    if (!item || item->data(0, RoleType).toString() != "session") return;

    const QUuid id = QUuid::fromString(item->data(0, RoleUuid).toString());
    auto profile = m_store->find(id);

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this, tr("Duplicate session"), tr("New session name:"),
        QLineEdit::Normal, profile.name + tr(" (copy)"), &ok);
    if (!ok || newName.isEmpty()) return;

    profile.id   = QUuid::createUuid();
    profile.name = newName;
    m_store->add(profile);
    m_store->save();
    buildTree();
}

// ─── Tools menu ───────────────────────────────────────────────────────────────

void LoginDialog::onImportSessions()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import sessions"), QDir::homePath(),
        tr("JSON files (*.json);;All files (*)"));
    if (path.isEmpty()) return;

    if (m_store->importJson(path)) {
        m_store->save();
        buildTree();
        QMessageBox::information(this, tr("Import"), tr("Sessions imported successfully."));
    } else {
        QMessageBox::warning(this, tr("Import"), tr("Failed to import sessions."));
    }
}

void LoginDialog::onExportSessions()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export sessions"), QDir::homePath() + "/linscp_sessions.json",
        tr("JSON files (*.json);;All files (*)"));
    if (path.isEmpty()) return;

    if (m_store->exportJson(path))
        QMessageBox::information(this, tr("Export"), tr("Sessions exported successfully."));
    else
        QMessageBox::warning(this, tr("Export"), tr("Failed to export sessions."));
}

} // namespace linscp::ui::dialogs
