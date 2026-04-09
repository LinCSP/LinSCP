#pragma once
#include <QMainWindow>
#include <memory>

class QSplitter;
class QComboBox;
class QAction;
class QLabel;

namespace linscp::core::session { class SessionManager; class SessionStore; }
namespace linscp::core::transfer { class TransferQueue; class TransferManager; }
namespace linscp::core::sftp { class SftpClient; }
namespace linscp::core::sync { class SyncEngine; }
namespace linscp::core::keys { class KeyManager; class KeyGenerator; }

namespace linscp::ui {

namespace panels  { class LocalPanel; class RemotePanel; }
namespace dialogs { class TransferPanel; }

/// Главное окно LinSCP.
///
/// Компоновка:
///   ┌─────────────────────────────────────────────┐
///   │  MenuBar                                    │
///   ├─────────────────────────────────────────────┤
///   │  ToolBar  [Session combo ▾] [Connect] [...]  │
///   ├──────────────────┬──────────────────────────┤
///   │  LocalPanel      │  RemotePanel              │
///   │  (левая)         │  (правая)                 │
///   ├──────────────────┴──────────────────────────┤
///   │  TransferPanel (очередь)                    │
///   ├─────────────────────────────────────────────┤
///   │  StatusBar                                  │
///   └─────────────────────────────────────────────┘
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onConnect();
    void onDisconnect();
    void onNewSession();
    void onManageSessions();
    void onManageKeys();
    void onSync();
    void onAbout();
    void onSessionConnected();
    void onSessionError(const QString &message);

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void restoreWindowState();
    void saveWindowState();

    // Core
    std::unique_ptr<core::session::SessionStore>   m_sessionStore;
    std::unique_ptr<core::session::SessionManager> m_sessionManager;
    std::unique_ptr<core::transfer::TransferQueue> m_transferQueue;
    std::unique_ptr<core::keys::KeyManager>        m_keyManager;
    std::unique_ptr<core::keys::KeyGenerator>      m_keyGenerator;

    // Runtime (создаются после подключения)
    core::sftp::SftpClient           *m_sftp           = nullptr;
    core::transfer::TransferManager  *m_transferManager = nullptr;
    core::sync::SyncEngine           *m_syncEngine      = nullptr;

    // UI
    panels::LocalPanel   *m_localPanel    = nullptr;
    panels::RemotePanel  *m_remotePanel   = nullptr;
    dialogs::TransferPanel *m_transferPanel = nullptr;

    QSplitter *m_mainSplitter    = nullptr;
    QSplitter *m_vertSplitter    = nullptr;
    QComboBox *m_sessionCombo    = nullptr;
    QAction   *m_connectAction   = nullptr;
    QAction   *m_disconnectAction = nullptr;
    QLabel    *m_statusLabel     = nullptr;
    QLabel    *m_connectionLabel = nullptr;
};

} // namespace linscp::ui
