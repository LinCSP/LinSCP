#include "main_window.h"
#include "panels/local_panel.h"
#include "panels/remote_panel.h"
#include "dialogs/transfer_panel.h"
#include "dialogs/session_dialog.h"
#include "dialogs/key_dialog.h"
#include "dialogs/sync_dialog.h"

#include "core/session/session_store.h"
#include "core/session/session_manager.h"
#include "core/session/session_profile.h"
#include "core/ssh/ssh_session.h"
#include "core/sftp/sftp_client.h"
#include "core/transfer/transfer_queue.h"
#include "core/transfer/transfer_manager.h"
#include "core/sync/sync_engine.h"
#include "core/keys/key_manager.h"
#include "core/keys/key_generator.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QComboBox>
#include <QLabel>
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

    // ── Core ──────────────────────────────────────────────────────────────────
    QDir().mkpath(QDir::homePath() + "/.config/linscp");

    m_sessionStore   = std::make_unique<core::session::SessionStore>(
        QDir::homePath() + "/.config/linscp/sessions.json");
    m_sessionStore->load();

    m_sessionManager = std::make_unique<core::session::SessionManager>(
        m_sessionStore.get(), this);

    m_transferQueue  = std::make_unique<core::transfer::TransferQueue>(this);
    m_keyManager     = std::make_unique<core::keys::KeyManager>(this);
    m_keyGenerator   = std::make_unique<core::keys::KeyGenerator>(this);

    connect(m_sessionManager.get(), &core::session::SessionManager::sessionOpened,
            this, [this](const QUuid &) { onSessionConnected(); });
    connect(m_sessionManager.get(), &core::session::SessionManager::sessionError,
            this, [this](const QUuid &, const QString &msg) { onSessionError(msg); });

    // ── UI ────────────────────────────────────────────────────────────────────
    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    restoreWindowState();
}

MainWindow::~MainWindow()
{
    saveWindowState();
    m_sessionManager->closeAll();
}

void MainWindow::setupUi()
{
    // Локальная панель (всегда активна)
    m_localPanel = new panels::LocalPanel(this);

    // Удалённая панель создаётся после подключения; заглушка до тех пор
    auto *remotePlaceholder = new QWidget(this);
    remotePlaceholder->setObjectName("RemotePlaceholder");

    // Горизонтальный сплиттер: local | remote
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->addWidget(m_localPanel);
    m_mainSplitter->addWidget(remotePlaceholder);
    m_mainSplitter->setSizes({500, 500});
    m_mainSplitter->setHandleWidth(4);

    // Панель передач (внизу)
    m_transferPanel = new dialogs::TransferPanel(m_transferQueue.get(), this);
    m_transferPanel->setMaximumHeight(200);

    // Вертикальный сплиттер: [panels] | [transfer queue]
    m_vertSplitter = new QSplitter(Qt::Vertical, this);
    m_vertSplitter->addWidget(m_mainSplitter);
    m_vertSplitter->addWidget(m_transferPanel);
    m_vertSplitter->setStretchFactor(0, 3);
    m_vertSplitter->setStretchFactor(1, 1);

    setCentralWidget(m_vertSplitter);
}

void MainWindow::setupMenuBar()
{
    // ── File ─────────────────────────────────────────────────────────────────
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(QIcon::fromTheme("network-connect"),
                        tr("&New Session…"), QKeySequence("Ctrl+N"),
                        this, &MainWindow::onNewSession);
    fileMenu->addAction(tr("&Manage Sessions…"),
                        this, &MainWindow::onManageSessions);
    fileMenu->addSeparator();
    m_connectAction    = fileMenu->addAction(QIcon::fromTheme("network-connect"),
                                             tr("&Connect"),
                                             QKeySequence("Ctrl+Return"),
                                             this, &MainWindow::onConnect);
    m_disconnectAction = fileMenu->addAction(QIcon::fromTheme("network-disconnect"),
                                             tr("&Disconnect"),
                                             this, &MainWindow::onDisconnect);
    m_disconnectAction->setEnabled(false);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    // ── Session ───────────────────────────────────────────────────────────────
    QMenu *sessionMenu = menuBar()->addMenu(tr("&Session"));
    sessionMenu->addAction(QIcon::fromTheme("view-refresh"),
                           tr("&Refresh Remote"), QKeySequence(Qt::Key_F5),
                           this, [this]() {
                               if (m_remotePanel) m_remotePanel->refresh();
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
    connect(showHidden, &QAction::toggled, m_localPanel, [this](bool on) {
        // LocalPanel использует QFileSystemModel::filter
        // m_localPanel model().setFilter(...)
        Q_UNUSED(on);
    });
    connect(showHidden, &QAction::toggled, this, [](bool on) {
        // TODO: if (m_remotePanel) m_remotePanel->model()->setShowHidden(on);
        Q_UNUSED(on);
    });

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
    connect(m_sessionStore.get(), &core::session::SessionStore::changed, this, refreshCombo);

    tb->addWidget(m_sessionCombo);
    tb->addAction(m_connectAction);
    tb->addAction(m_disconnectAction);
    tb->addSeparator();
    tb->addAction(QIcon::fromTheme("folder-sync"), tr("Sync"),
                  this, &MainWindow::onSync);
}

void MainWindow::setupStatusBar()
{
    m_connectionLabel = new QLabel(tr("Not connected"), this);
    m_statusLabel     = new QLabel(this);
    statusBar()->addWidget(m_connectionLabel);
    statusBar()->addPermanentWidget(m_statusLabel);
}

// ── Слоты ─────────────────────────────────────────────────────────────────────

void MainWindow::onConnect()
{
    if (m_sessionCombo->currentIndex() < 0) {
        onNewSession();
        return;
    }

    const QUuid profileId = QUuid::fromString(
        m_sessionCombo->currentData().toString());

    auto *sshSession = m_sessionManager->open(profileId);
    if (!sshSession) return;

    // SftpClient создаётся после SSH-коннекта в onSessionConnected()
    m_connectionLabel->setText(tr("Connecting…"));
}

void MainWindow::onDisconnect()
{
    const QUuid profileId = QUuid::fromString(
        m_sessionCombo->currentData().toString());
    m_sessionManager->close(profileId);

    // Убрать remote панель
    if (m_remotePanel) {
        m_mainSplitter->replaceWidget(1, new QWidget(this));
        m_remotePanel->deleteLater();
        m_remotePanel = nullptr;
    }

    m_connectAction->setEnabled(true);
    m_disconnectAction->setEnabled(false);
    m_connectionLabel->setText(tr("Disconnected"));
}

void MainWindow::onSessionConnected()
{
    const QUuid profileId = QUuid::fromString(
        m_sessionCombo->currentData().toString());

    auto *sshSession = m_sessionManager->active(profileId);
    if (!sshSession) return;

    // Создать SFTP-клиент на этой сессии
    m_sftp = new core::sftp::SftpClient(sshSession, this);

    // Создать менеджер передач
    m_transferManager = new core::transfer::TransferManager(
        m_sftp, m_transferQueue.get(), this);
    m_transferManager->start();

    // Создать движок синхронизации
    m_syncEngine = new core::sync::SyncEngine(m_sftp, m_transferQueue.get(), this);

    // Создать/заменить remote-панель
    if (m_remotePanel) {
        m_remotePanel->deleteLater();
    }
    m_remotePanel = new panels::RemotePanel(m_sftp, m_transferQueue.get(), this);
    m_mainSplitter->replaceWidget(1, m_remotePanel);

    // Настроить drag & drop между панелями
    connect(m_localPanel, &panels::LocalPanel::uploadRequested,
            this, [this](const QStringList &files, const QString & /*dest*/) {
                if (m_remotePanel) m_remotePanel->uploadFiles(files);
            });
    connect(m_remotePanel, &panels::RemotePanel::downloadRequested,
            this, [this](const QStringList &files, const QString &dest) {
                Q_UNUSED(files); Q_UNUSED(dest);
                if (m_remotePanel) m_remotePanel->downloadSelected(m_localPanel->currentPath());
            });

    m_connectAction->setEnabled(false);
    m_disconnectAction->setEnabled(true);

    const auto profile = m_sessionStore->find(profileId);
    m_connectionLabel->setText(
        tr("Connected: %1@%2").arg(profile.username, profile.host));

    if (!profile.initialRemotePath.isEmpty())
        m_remotePanel->navigateTo(profile.initialRemotePath);
}

void MainWindow::onSessionError(const QString &message)
{
    m_connectionLabel->setText(tr("Error"));
    QMessageBox::warning(this, tr("Connection Error"), message);
    m_connectAction->setEnabled(true);
    m_disconnectAction->setEnabled(false);
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
    // TODO: SessionManagerDialog (список + CRUD)
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
    if (!m_syncEngine) {
        QMessageBox::information(this, tr("Sync"),
                                  tr("Connect to a remote server first."));
        return;
    }

    dialogs::SyncDialog dlg(m_syncEngine,
                             m_localPanel->currentPath(),
                             m_remotePanel ? m_remotePanel->currentPath() : "/",
                             this);
    dlg.exec();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About LinSCP"),
                        tr("<h2>LinSCP v0.1</h2>"
                           "<p>Cross-platform SFTP/SSH file manager.</p>"
                           "<p>Open source · LGPL 2.1<br>"
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
    s.setValue("geometry",        saveGeometry());
    s.setValue("windowState",     saveState());
    s.setValue("mainSplitter",    m_mainSplitter->saveState());
    s.setValue("vertSplitter",    m_vertSplitter->saveState());
}

void MainWindow::restoreWindowState()
{
    QSettings s("LinSCP", "LinSCP");
    if (s.contains("geometry"))     restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("windowState"))  restoreState(s.value("windowState").toByteArray());
    if (s.contains("mainSplitter")) m_mainSplitter->restoreState(s.value("mainSplitter").toByteArray());
    if (s.contains("vertSplitter")) m_vertSplitter->restoreState(s.value("vertSplitter").toByteArray());
}

} // namespace linscp::ui
