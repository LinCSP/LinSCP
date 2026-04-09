#include "main_window.h"
#include "connection_tab.h"
#include "dialogs/transfer_panel.h"
#include "dialogs/session_dialog.h"
#include "dialogs/key_dialog.h"
#include "dialogs/sync_dialog.h"
#include "panels/local_panel.h"
#include "panels/remote_panel.h"

#include "core/session/session_store.h"
#include "core/transfer/transfer_queue.h"
#include "core/keys/key_manager.h"
#include "core/keys/key_generator.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QShortcut>
#include <QTabWidget>
#include <QTabBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QComboBox>
#include <QLabel>
#include <QToolButton>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QCloseEvent>

namespace linscp::ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("LinSCP");
    setWindowIcon(QIcon::fromTheme("network-server"));
    setMinimumSize(900, 600);

    QDir().mkpath(QDir::homePath() + "/.config/linscp");

    m_sessionStore  = std::make_unique<core::session::SessionStore>(
        QDir::homePath() + "/.config/linscp/sessions.json");
    m_sessionStore->load();

    m_transferQueue = std::make_unique<core::transfer::TransferQueue>(this);
    m_keyManager    = std::make_unique<core::keys::KeyManager>(this);
    m_keyGenerator  = std::make_unique<core::keys::KeyGenerator>(this);

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
    m_tabWidget->setTabsClosable(true);
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
}

void MainWindow::setupMenuBar()
{
    // ── File ─────────────────────────────────────────────────────────────────
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(QIcon::fromTheme("tab-new"),
                        tr("&New Tab"), QKeySequence("Ctrl+T"),
                        this, &MainWindow::onNewTab);
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon::fromTheme("network-connect"),
                        tr("&New Session…"), QKeySequence("Ctrl+N"),
                        this, &MainWindow::onNewSession);
    fileMenu->addAction(tr("&Manage Sessions…"),
                        this, &MainWindow::onManageSessions);
    fileMenu->addSeparator();
    m_connectAction = fileMenu->addAction(
        QIcon::fromTheme("network-connect"), tr("&Connect"),
        QKeySequence("Ctrl+Return"), this, &MainWindow::onConnect);
    m_disconnectAction = fileMenu->addAction(
        QIcon::fromTheme("network-disconnect"), tr("&Disconnect"),
        this, &MainWindow::onDisconnect);
    m_disconnectAction->setEnabled(false);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit,
                        qApp, &QApplication::quit);

    // ── Session ───────────────────────────────────────────────────────────────
    QMenu *sessionMenu = menuBar()->addMenu(tr("&Session"));
    sessionMenu->addAction(QIcon::fromTheme("view-refresh"),
                           tr("&Refresh Remote"), QKeySequence(Qt::Key_F5),
                           this, [this]() {
        if (auto *tab = currentTab(); tab && tab->remotePanel())
            tab->remotePanel()->refresh();
    });

    // ── Commands ──────────────────────────────────────────────────────────────
    QMenu *cmdMenu = menuBar()->addMenu(tr("&Commands"));
    cmdMenu->addAction(QIcon::fromTheme("folder-sync"),
                       tr("&Synchronize…"), QKeySequence("Ctrl+Shift+S"),
                       this, &MainWindow::onSync);
    cmdMenu->addAction(tr("&SSH Key Manager…"),
                       this, &MainWindow::onManageKeys);

    // ── View ──────────────────────────────────────────────────────────────────
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    QAction *showHidden = viewMenu->addAction(tr("Show &Hidden Files"));
    showHidden->setCheckable(true);
    connect(showHidden, &QAction::toggled, this, [](bool) { /* TODO */ });

    // ── Help ──────────────────────────────────────────────────────────────────
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About LinSCP"), this, &MainWindow::onAbout);
    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
}

void MainWindow::setupToolBar()
{
    QToolBar *tb = addToolBar(tr("Main Toolbar"));
    tb->setMovable(false);
    tb->setIconSize(QSize(20, 20));

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
    tb->addAction(QIcon::fromTheme("folder-sync"), tr("Sync"),
                  this, &MainWindow::onSync);
    tb->addSeparator();
    tb->addAction(QIcon::fromTheme("tab-new"), tr("New Tab"),
                  this, &MainWindow::onNewTab);
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

    new QShortcut(Qt::Key_F5, this, [activePanel]() {
        if (auto *p = activePanel()) p->refresh();
    });
    new QShortcut(Qt::Key_F6, this, [activePanel]() {
        if (auto *p = activePanel()) p->actionRename();
    });
    new QShortcut(Qt::Key_F7, this, [activePanel]() {
        if (auto *p = activePanel()) p->actionMkdir();
    });
    new QShortcut(Qt::Key_F8, this, [activePanel]() {
        if (auto *p = activePanel()) p->actionDelete();
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
    auto *tab = new ConnectionTab(m_sessionStore.get(), m_transferQueue.get(), this);
    const QString tabTitle = title.isEmpty() ? tr("New connection") : title;
    const int idx = m_tabWidget->addTab(tab, tabTitle);
    m_tabWidget->setCurrentIndex(idx);

    connect(tab, &ConnectionTab::titleChanged, this, [this, tab](const QString &t) {
        const int i = m_tabWidget->indexOf(tab);
        if (i >= 0) m_tabWidget->setTabText(i, t);
    });
    connect(tab, &ConnectionTab::statusChanged, this, [this, tab](const QString &msg) {
        if (tab == currentTab()) m_connectionLabel->setText(msg);
    });
    connect(tab, &ConnectionTab::connectionEstablished, this, [this]() {
        updateToolbarState();
    });
    connect(tab, &ConnectionTab::connectionLost, this, [this]() {
        updateToolbarState();
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
    addConnectionTab();
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
    dialogs::SessionDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    const auto profile = dlg.profile();
    if (!profile.isValid()) return;
    m_sessionStore->add(profile);
    m_sessionStore->save();
}

void MainWindow::onManageSessions()
{
    QMessageBox::information(this, tr("Sessions"),
                              tr("Session manager UI — coming soon."));
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
                             remotePath, this);
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
}

void MainWindow::restoreWindowState()
{
    QSettings s("LinSCP", "LinSCP");
    if (s.contains("geometry"))     restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("windowState"))  restoreState(s.value("windowState").toByteArray());
    if (s.contains("vertSplitter")) m_vertSplitter->restoreState(
        s.value("vertSplitter").toByteArray());
}

} // namespace linscp::ui
