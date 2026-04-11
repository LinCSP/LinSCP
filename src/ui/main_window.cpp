#include "main_window.h"
#include "connection_tab.h"
#include "dialogs/transfer_panel.h"
#include "dialogs/session_dialog.h"
#include "dialogs/login_dialog.h"
#include "dialogs/key_dialog.h"
#include "dialogs/sync_dialog.h"
#include "panels/local_panel.h"
#include "panels/remote_panel.h"
#include "terminal/terminal_widget.h"
#include "dialogs/preferences_dialog.h"

#include "core/session/session_store.h"
#include "core/session/path_state_store.h"
#include "core/transfer/transfer_queue.h"
#include "core/keys/key_manager.h"
#include "core/keys/key_generator.h"
#include "core/sync/sync_profile_store.h"

#include "utils/svg_icon.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QShortcut>
#include <QTabWidget>
#include <QTabBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QDockWidget>
#include <QComboBox>
#include <QLabel>
#include <QToolButton>
#include <QActionGroup>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QCloseEvent>

namespace linscp::ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("LinSCP");
    setWindowIcon(svgIcon(QStringLiteral("server")));
    setMinimumSize(900, 600);

    QDir().mkpath(QDir::homePath() + "/.config/linscp");

    m_sessionStore  = std::make_unique<core::session::SessionStore>(
        QDir::homePath() + "/.config/linscp/sessions.json");
    m_sessionStore->load();

    m_pathStateStore = std::make_unique<core::session::PathStateStore>(
        QDir::homePath() + "/.config/linscp/path_state.json", this);

    m_transferQueue     = std::make_unique<core::transfer::TransferQueue>(this);
    m_keyManager        = std::make_unique<core::keys::KeyManager>(this);
    m_keyGenerator      = std::make_unique<core::keys::KeyGenerator>(this);
    m_syncProfileStore  = std::make_unique<core::sync::SyncProfileStore>(
        QDir::homePath() + "/.config/linscp/sync_profiles.json", this);
    m_syncProfileStore->load();

    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupHotkeys();
    restoreWindowState();
}

MainWindow::~MainWindow()
{
    saveWindowState();
    // Вкладки разрушатся сами (children of m_tabWidget),
    // каждая вызовет disconnectSession() в деструкторе
}

// ── Построение UI ─────────────────────────────────────────────────────────────

void MainWindow::setupUi()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(false); // кастомные кнопки добавляем вручную
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);

    // Кнопка "+" для нового таба
    auto *newTabBtn = new QToolButton(m_tabWidget);
    newTabBtn->setText("+");
    newTabBtn->setToolTip(tr("New connection tab"));
    newTabBtn->setAutoRaise(true);
    m_tabWidget->setCornerWidget(newTabBtn, Qt::TopRightCorner);
    connect(newTabBtn, &QToolButton::clicked, this, &MainWindow::onNewTab);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab);
    connect(m_tabWidget, &QTabWidget::currentChanged,    this, &MainWindow::onTabChanged);

    // Добавить первый пустой таб
    addConnectionTab();

    // Панель передач (общая для всех табов)
    m_transferPanel = new dialogs::TransferPanel(m_transferQueue.get(), this);
    m_transferPanel->setMaximumHeight(180);

    // Вертикальный сплиттер: [tabWidget] | [transferPanel]
    m_vertSplitter = new QSplitter(Qt::Vertical, this);
    m_vertSplitter->addWidget(m_tabWidget);
    m_vertSplitter->addWidget(m_transferPanel);
    m_vertSplitter->setStretchFactor(0, 4);
    m_vertSplitter->setStretchFactor(1, 1);

    setCentralWidget(m_vertSplitter);

    // ── Terminal Dock ─────────────────────────────────────────────────────────
    // QDockWidget: по умолчанию снизу, пользователь может оторвать drag'ом
    m_terminalWidget = new terminal::TerminalWidget(this);

    m_terminalDock = new QDockWidget(tr("Terminal"), this);
    m_terminalDock->setObjectName("TerminalDock");   // для saveState/restoreState
    m_terminalDock->setWidget(m_terminalWidget);
    m_terminalDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    // Разрешить float (отрыв в отдельное окно) и закрытие
    m_terminalDock->setFeatures(QDockWidget::DockWidgetMovable   |
                                QDockWidget::DockWidgetFloatable |
                                QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
    m_terminalDock->hide();   // скрыт по умолчанию

    // Синхронизировать checkable-кнопку с состоянием дока
    connect(m_terminalDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_terminalAction) m_terminalAction->setChecked(visible);
        if (visible) attachTerminalToCurrentTab();
    });
}

void MainWindow::setupMenuBar()
{
    // ── File ─────────────────────────────────────────────────────────────────
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(svgIcon(QStringLiteral("file-circle-plus")),
                        tr("&New Tab"), QKeySequence("Ctrl+T"),
                        this, &MainWindow::onNewTab);
    fileMenu->addSeparator();
    fileMenu->addAction(svgIcon(QStringLiteral("plug")),
                        tr("&New Session…"), QKeySequence("Ctrl+N"),
                        this, &MainWindow::onNewSession);
    fileMenu->addAction(tr("&Manage Sessions…"),
                        this, &MainWindow::onManageSessions);
    fileMenu->addSeparator();
    m_connectAction = fileMenu->addAction(
        svgIcon(QStringLiteral("plug")), tr("&Connect"),
        QKeySequence("Ctrl+Return"), this, &MainWindow::onConnect);
    m_disconnectAction = fileMenu->addAction(
        svgIcon(QStringLiteral("plug-circle-xmark")), tr("&Disconnect"),
        this, &MainWindow::onDisconnect);
    m_disconnectAction->setEnabled(false);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit,
                        qApp, &QApplication::quit);

    // ── Session ───────────────────────────────────────────────────────────────
    QMenu *sessionMenu = menuBar()->addMenu(tr("&Session"));
    sessionMenu->addAction(svgIcon(QStringLiteral("rotate")),
                           tr("&Refresh Remote"), QKeySequence("Ctrl+R"),
                           this, [this]() {
        if (auto *tab = currentTab(); tab && tab->remotePanel())
            tab->remotePanel()->refresh();
    });
    sessionMenu->addSeparator();
    m_terminalAction = sessionMenu->addAction(
        svgIcon(QStringLiteral("terminal")),
        tr("Open &Terminal"), QKeySequence(Qt::Key_F9),
        this, &MainWindow::onToggleTerminal);
    m_terminalAction->setCheckable(true);

    // ── Commands ──────────────────────────────────────────────────────────────
    QMenu *cmdMenu = menuBar()->addMenu(tr("&Commands"));
    cmdMenu->addAction(svgIcon(QStringLiteral("arrows-rotate")),
                       tr("&Synchronize…"), QKeySequence("Ctrl+Shift+S"),
                       this, &MainWindow::onSync);
    cmdMenu->addAction(tr("&SSH Key Manager…"),
                       this, &MainWindow::onManageKeys);

    // ── View ──────────────────────────────────────────────────────────────────
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    m_showHiddenAction = viewMenu->addAction(tr("Show &Hidden Files"));
    m_showHiddenAction->setCheckable(true);
    m_showHiddenAction->setShortcut(QKeySequence("Ctrl+Alt+H"));
    connect(m_showHiddenAction, &QAction::toggled, this, [this](bool show) {
        // Применяем к обеим панелям текущего таба
        if (auto *tab = currentTab()) {
            if (tab->localPanel())
                tab->localPanel()->setShowHiddenFiles(show);
            if (tab->remotePanel())
                tab->remotePanel()->setShowHiddenFiles(show);
        }
    });

    viewMenu->addSeparator();

    // Подменю выбора языка — список доступных переводов
    QMenu *langMenu = viewMenu->addMenu(tr("&Language"));
    auto *langGroup = new QActionGroup(langMenu);
    langGroup->setExclusive(true);

    struct LangEntry { QString code; QString label; };
    const QList<LangEntry> languages = {
        { "en", "English" },
        { "ru", "Русский" },
    };

    const QSettings prefs("LinSCP", "LinSCP");
    const QString currentLang = prefs.value("language").toString();

    for (const auto &l : languages) {
        auto *act = langMenu->addAction(l.label);
        act->setCheckable(true);
        act->setActionGroup(langGroup);
        act->setChecked(l.code == currentLang ||
                        (l.code == "en" && currentLang.isEmpty()));
        const QString code = l.code;
        connect(act, &QAction::triggered, this, [code]() {
            QSettings s("LinSCP", "LinSCP");
            s.setValue("language", code);
            QMessageBox::information(
                nullptr,
                tr("Language"),
                tr("Language change will take effect after restarting LinSCP."));
        });
    }

    // ── Options ───────────────────────────────────────────────────────────────
    QMenu *optMenu = menuBar()->addMenu(tr("&Options"));
    optMenu->addAction(svgIcon(QStringLiteral("gear")),
                       tr("&Preferences…"), QKeySequence("Ctrl+,"),
                       this, &MainWindow::onPreferences);

    // ── Help ──────────────────────────────────────────────────────────────────
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About LinSCP"), this, &MainWindow::onAbout);
    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
}

void MainWindow::setupToolBar()
{
    QToolBar *tb = addToolBar(tr("Main Toolbar"));
    tb->setMovable(false);
    tb->setIconSize(QSize(18, 18));

    // Комбо-бокс сессий
    m_sessionCombo = new QComboBox(tb);
    m_sessionCombo->setMinimumWidth(200);
    m_sessionCombo->setPlaceholderText(tr("Select session…"));

    auto refreshCombo = [this]() {
        m_sessionCombo->clear();
        for (const auto &p : m_sessionStore->all())
            m_sessionCombo->addItem(p.name, p.id.toString());
    };
    refreshCombo();
    connect(m_sessionStore.get(), &core::session::SessionStore::changed,
            this, refreshCombo);

    tb->addWidget(m_sessionCombo);
    tb->addAction(m_connectAction);
    tb->addAction(m_disconnectAction);
    tb->addSeparator();
    tb->addAction(svgIcon(QStringLiteral("arrows-rotate")),
                  tr("Sync"), this, &MainWindow::onSync);
    tb->addSeparator();
    tb->addAction(m_terminalAction);
    tb->addSeparator();
    tb->addAction(svgIcon(QStringLiteral("file-circle-plus")),
                  tr("New Tab"), this, &MainWindow::onNewTab);
}

void MainWindow::setupStatusBar()
{
    m_connectionLabel = new QLabel(tr("Not connected"), this);
    m_statusLabel     = new QLabel(this);
    statusBar()->addWidget(m_connectionLabel);
    statusBar()->addPermanentWidget(m_statusLabel);
}

void MainWindow::setupHotkeys()
{
    auto activePanel = [this]() -> panels::FilePanel * {
        auto *tab = currentTab();
        if (!tab) return nullptr;
        QWidget *focus = QApplication::focusWidget();
        if (tab->remotePanel() &&
            (tab->remotePanel() == focus || tab->remotePanel()->isAncestorOf(focus)))
            return tab->remotePanel();
        return tab->localPanel();
    };

    // F2 — переименовать (как в WinSCP и стандартных файловых менеджерах)
    new QShortcut(Qt::Key_F2, this, [activePanel]() {
        if (auto *p = activePanel()) p->actionRename();
    });
    // F5 — копировать / загрузить / скачать
    new QShortcut(Qt::Key_F5, this, [activePanel]() {
        if (auto *p = activePanel()) p->actionCopy();
    });
    // F6 — переместить
    new QShortcut(Qt::Key_F6, this, [activePanel]() {
        if (auto *p = activePanel()) p->actionMove();
    });
    // F7 — новая папка
    new QShortcut(Qt::Key_F7, this, [activePanel]() {
        if (auto *p = activePanel()) p->actionMkdir();
    });
    // F8 / Delete — удалить
    new QShortcut(Qt::Key_F8, this, [activePanel]() {
        if (auto *p = activePanel()) p->actionDelete();
    });
    new QShortcut(Qt::Key_Delete, this, [activePanel]() {
        if (auto *p = activePanel()) p->actionDelete();
    });
    // Ctrl+R — обновить панель
    new QShortcut(QKeySequence("Ctrl+R"), this, [activePanel]() {
        if (auto *p = activePanel()) p->refresh();
    });
    // Tab — переключить левая/правая панель
    new QShortcut(Qt::Key_Tab, this, [this, activePanel]() {
        auto *tab = currentTab();
        if (!tab || !tab->remotePanel()) return;
        QWidget *focus = QApplication::focusWidget();
        if (tab->localPanel()->isAncestorOf(focus) || tab->localPanel() == focus)
            tab->remotePanel()->setFocused();
        else
            tab->localPanel()->setFocused();
    });
    // Ctrl+W — закрыть текущий таб
    new QShortcut(QKeySequence("Ctrl+W"), this, [this]() {
        onCloseTab(m_tabWidget->currentIndex());
    });
}

// ── Управление табами ─────────────────────────────────────────────────────────

ConnectionTab *MainWindow::addConnectionTab(const QString &title)
{
    auto *tab = new ConnectionTab(m_sessionStore.get(), m_transferQueue.get(),
                                  m_pathStateStore.get(), this);
    const QString tabTitle = title.isEmpty() ? tr("New connection") : title;
    const int idx = m_tabWidget->addTab(tab, tabTitle);
    m_tabWidget->setCurrentIndex(idx);

    // Кастомная кнопка закрытия таба с FA-иконкой
    auto *closeBtn = new QToolButton(m_tabWidget);
    closeBtn->setIcon(svgIcon(QStringLiteral("xmark")));
    closeBtn->setIconSize(QSize(10, 10));
    closeBtn->setFixedSize(QSize(16, 16));
    closeBtn->setAutoRaise(true);
    closeBtn->setToolTip(tr("Close tab"));
    connect(closeBtn, &QToolButton::clicked, this, [this, tab]() {
        onCloseTab(m_tabWidget->indexOf(tab));
    });
    m_tabWidget->tabBar()->setTabButton(idx, QTabBar::RightSide, closeBtn);

    // Применяем текущее состояние «показывать скрытые» к новому табу
    if (m_showHiddenAction && m_showHiddenAction->isChecked()) {
        if (tab->localPanel())
            tab->localPanel()->setShowHiddenFiles(true);
        if (tab->remotePanel())
            tab->remotePanel()->setShowHiddenFiles(true);
    }

    connect(tab, &ConnectionTab::titleChanged, this, [this, tab](const QString &t) {
        const int i = m_tabWidget->indexOf(tab);
        if (i >= 0) m_tabWidget->setTabText(i, t);
    });
    connect(tab, &ConnectionTab::statusChanged, this, [this, tab](const QString &msg) {
        if (tab == currentTab()) m_connectionLabel->setText(msg);
    });
    connect(tab, &ConnectionTab::connectionEstablished, this, [this, tab]() {
        updateToolbarState();
        // Применяем showHidden к новой RemotePanel, которая создаётся при коннекте
        if (m_showHiddenAction && m_showHiddenAction->isChecked() && tab->remotePanel())
            tab->remotePanel()->setShowHiddenFiles(true);
        // Если терминал виден — открыть shell на новой сессии
        if (m_terminalDock && m_terminalDock->isVisible())
            attachTerminalToCurrentTab();
    });
    connect(tab, &ConnectionTab::connectionLost, this, [this]() {
        updateToolbarState();
        // Отвязать терминал от закрытой сессии
        if (m_terminalWidget)
            m_terminalWidget->setConnectionInfo({}, 22, {});
    });

    return tab;
}

ConnectionTab *MainWindow::currentTab() const
{
    return qobject_cast<ConnectionTab *>(m_tabWidget->currentWidget());
}

void MainWindow::updateToolbarState()
{
    // Может вызываться до setupMenuBar/setupStatusBar — проверяем nullptr
    if (!m_connectAction || !m_disconnectAction) return;
    auto *tab = currentTab();
    const bool connected = tab && tab->isConnected();
    m_connectAction->setEnabled(!connected);
    m_disconnectAction->setEnabled(connected);
    if (m_connectionLabel && tab)
        m_connectionLabel->setText(tab->title());
}

// ── Слоты ─────────────────────────────────────────────────────────────────────

void MainWindow::onNewTab()
{
    dialogs::LoginDialog dlg(m_sessionStore.get(), this);
    if (dlg.exec() != QDialog::Accepted) return;

    const auto profile = dlg.selectedProfile();
    if (!profile.isValid()) return;

    auto *tab = addConnectionTab(profile.name);
    tab->connectToProfile(profile);
    updateToolbarState();
}

void MainWindow::onCloseTab(int index)
{
    if (m_tabWidget->count() <= 1) {
        // Оставляем хотя бы один таб, просто дисконнектим
        if (auto *tab = currentTab())
            tab->disconnectSession();
        return;
    }
    auto *tab = qobject_cast<ConnectionTab *>(m_tabWidget->widget(index));
    m_tabWidget->removeTab(index);
    delete tab;
    updateToolbarState();
}

void MainWindow::onTabChanged(int /*index*/)
{
    updateToolbarState();
    // Если терминал виден — переключить на сессию нового таба
    if (m_terminalDock && m_terminalDock->isVisible())
        attachTerminalToCurrentTab();
}

void MainWindow::onConnect()
{
    if (m_sessionCombo->currentIndex() < 0) {
        onNewSession();
        return;
    }
    auto *tab = currentTab();
    if (!tab) tab = addConnectionTab();

    const QUuid profileId = QUuid::fromString(
        m_sessionCombo->currentData().toString());
    tab->connectToSession(profileId);
    updateToolbarState();
}

void MainWindow::onDisconnect()
{
    if (auto *tab = currentTab())
        tab->disconnectSession();
    updateToolbarState();
}

void MainWindow::onNewSession()
{
    dialogs::LoginDialog dlg(m_sessionStore.get(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    const auto profile = dlg.selectedProfile();
    if (!profile.isValid()) return;
    auto *tab = addConnectionTab(profile.name);
    tab->connectToProfile(profile);
    updateToolbarState();
}

void MainWindow::onManageSessions()
{
    dialogs::LoginDialog dlg(m_sessionStore.get(), this);
    dlg.exec();
    // Обновить комбо-бокс сессий (сигнал changed() уже сделает это автоматически)
}

void MainWindow::onManageKeys()
{
    dialogs::KeyDialog dlg(m_keyManager.get(), m_keyGenerator.get(), this);
    dlg.exec();
}

void MainWindow::onSync()
{
    auto *tab = currentTab();
    if (!tab || !tab->syncEngine()) {
        QMessageBox::information(this, tr("Sync"),
                                  tr("Connect to a remote server first."));
        return;
    }
    const QString remotePath = tab->remotePanel()
                                   ? tab->remotePanel()->currentPath()
                                   : "/";
    dialogs::SyncDialog dlg(tab->syncEngine(),
                             tab->localPanel()->currentPath(),
                             remotePath,
                             m_syncProfileStore.get(),
                             this);
    dlg.exec();
}

void MainWindow::onToggleTerminal()
{
    if (!m_terminalDock) return;
    if (m_terminalDock->isVisible()) {
        m_terminalDock->hide();
    } else {
        m_terminalDock->show();
        m_terminalWidget->setFocus();
    }
}

void MainWindow::attachTerminalToCurrentTab()
{
    if (!m_terminalWidget) return;
    auto *tab = currentTab();

    if (!tab || tab->profileId().isNull()) {
        m_terminalWidget->setConnectionInfo({}, 22, {});
        return;
    }

    const auto profile = m_sessionStore->find(tab->profileId());
    if (profile.isValid()) {
        m_terminalWidget->setConnectionInfo(
            profile.host, profile.port, profile.username,
            profile.privateKeyPath, profile.password);
        // Авто-запуск если dok только что открыт и сессия уже подключена
        if (tab->isConnected() && !m_terminalWidget->isShellOpen())
            m_terminalWidget->openShell();
    }
}

void MainWindow::onPreferences()
{
    dialogs::PreferencesDialog dlg(this);
    dlg.exec();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About LinSCP"),
                        tr("<h2>LinSCP v0.1</h2>"
                           "<p>Cross-platform SFTP/SSH file manager.</p>"
                           "<p>Open source · GPL v2<br>"
                           "<a href=\"https://github.com/\">GitHub</a></p>"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowState();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveWindowState()
{
    QSettings s("LinSCP", "LinSCP");
    s.setValue("geometry",    saveGeometry());
    s.setValue("windowState", saveState());
    s.setValue("vertSplitter", m_vertSplitter->saveState());
    if (m_showHiddenAction)
        s.setValue("showHiddenFiles", m_showHiddenAction->isChecked());
}

void MainWindow::restoreWindowState()
{
    QSettings s("LinSCP", "LinSCP");
    if (s.contains("geometry"))     restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("windowState"))  restoreState(s.value("windowState").toByteArray());
    if (s.contains("vertSplitter")) m_vertSplitter->restoreState(
        s.value("vertSplitter").toByteArray());
    if (m_showHiddenAction && s.contains("showHiddenFiles"))
        m_showHiddenAction->setChecked(s.value("showHiddenFiles").toBool());
}

} // namespace linscp::ui
