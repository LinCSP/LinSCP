#pragma once
#include <QMainWindow>
#include <memory>

class QSplitter;
class QTabWidget;
class QComboBox;
class QAction;
class QLabel;
class QDockWidget;

namespace linscp::core::session { class SessionStore; }
namespace linscp::core::transfer { class TransferQueue; }
namespace linscp::core::keys    { class KeyManager; class KeyGenerator; }

namespace linscp::ui {

class ConnectionTab;
namespace dialogs  { class TransferPanel; }
namespace terminal { class TerminalWidget; }

/// Главное окно LinSCP.
///
///   ┌─────────────────────────────────────────────────────────┐
///   │  MenuBar                                                │
///   ├─────────────────────────────────────────────────────────┤
///   │  ToolBar  [Session combo ▾] [Connect] [Disconnect] [...] │
///   ├──────────┬──────────┬──────────┬────────────────────────┤
///   │ Server1  │ Server2  │    +     │       ← QTabWidget     │
///   ├──────────┴──────────┴──────────┴────────────────────────┤
///   │  LocalPanel         │   RemotePanel                     │
///   ├─────────────────────┴───────────────────────────────────┤
///   │  TransferPanel (общая очередь)                          │
///   ├─────────────────────────────────────────────────────────┤
///   │  [Terminal Dock] — снизу, отрываемый в отдельное окно   │
///   ├─────────────────────────────────────────────────────────┤
///   │  StatusBar                                              │
///   └─────────────────────────────────────────────────────────┘
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewTab();
    void onCloseTab(int index);
    void onTabChanged(int index);
    void onConnect();
    void onDisconnect();
    void onNewSession();
    void onManageSessions();
    void onManageKeys();
    void onSync();
    void onToggleTerminal();
    void onAbout();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupHotkeys();
    void restoreWindowState();
    void saveWindowState();

    ConnectionTab *addConnectionTab(const QString &title = {});
    ConnectionTab *currentTab() const;
    void updateToolbarState();

    /// Привязать терминал к SSH-сессии текущего таба
    void attachTerminalToCurrentTab();

    // Core
    std::unique_ptr<core::session::SessionStore>   m_sessionStore;
    std::unique_ptr<core::transfer::TransferQueue> m_transferQueue;
    std::unique_ptr<core::keys::KeyManager>        m_keyManager;
    std::unique_ptr<core::keys::KeyGenerator>      m_keyGenerator;

    // UI
    QTabWidget             *m_tabWidget     = nullptr;
    dialogs::TransferPanel *m_transferPanel = nullptr;
    QSplitter              *m_vertSplitter  = nullptr;

    // Terminal dock — один на всё приложение, переключается при смене таба
    QDockWidget              *m_terminalDock   = nullptr;
    terminal::TerminalWidget *m_terminalWidget = nullptr;

    QComboBox *m_sessionCombo     = nullptr;
    QAction   *m_connectAction    = nullptr;
    QAction   *m_disconnectAction = nullptr;
    QAction   *m_terminalAction   = nullptr;
    QLabel    *m_connectionLabel  = nullptr;
    QLabel    *m_statusLabel      = nullptr;
};

} // namespace linscp::ui
