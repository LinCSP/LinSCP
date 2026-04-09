#pragma once
#include <QMainWindow>
#include <memory>

class QSplitter;
class QTabWidget;
class QComboBox;
class QAction;
class QLabel;

namespace linscp::core::session { class SessionStore; }
namespace linscp::core::transfer { class TransferQueue; }
namespace linscp::core::keys    { class KeyManager; class KeyGenerator; }

namespace linscp::ui {

class ConnectionTab;
namespace dialogs { class TransferPanel; }

/// Главное окно LinSCP.
///
///   ┌─────────────────────────────────────────────────────────┐
///   │  MenuBar                                                │
///   ├─────────────────────────────────────────────────────────┤
///   │  ToolBar  [Session combo ▾] [Connect] [Disconnect] [...] │
///   ├──────────┬──────────┬──────────┬────────────────────────┤
///   │ Server1  │ Server2  │    +     │                        │  ← QTabWidget
///   ├──────────┴──────────┴──────────┴────────────────────────┤
///   │  LocalPanel         │   RemotePanel (своя у каждого таба)│
///   ├─────────────────────┴───────────────────────────────────┤
///   │  TransferPanel (общая очередь)                          │
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
    void onAbout();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupHotkeys();
    void restoreWindowState();
    void saveWindowState();

    /// Создать новый таб (пустой) и вернуть указатель
    ConnectionTab *addConnectionTab(const QString &title = {});
    /// Активный таб (может быть nullptr если табов нет)
    ConnectionTab *currentTab() const;
    /// Обновить состояние кнопок Connect/Disconnect под текущий таб
    void updateToolbarState();

    // Core (общие для всех табов)
    std::unique_ptr<core::session::SessionStore>   m_sessionStore;
    std::unique_ptr<core::transfer::TransferQueue> m_transferQueue;
    std::unique_ptr<core::keys::KeyManager>        m_keyManager;
    std::unique_ptr<core::keys::KeyGenerator>      m_keyGenerator;

    // UI
    QTabWidget              *m_tabWidget     = nullptr;
    dialogs::TransferPanel  *m_transferPanel = nullptr;
    QSplitter               *m_vertSplitter  = nullptr;

    QComboBox *m_sessionCombo     = nullptr;
    QAction   *m_connectAction    = nullptr;
    QAction   *m_disconnectAction = nullptr;
    QLabel    *m_connectionLabel  = nullptr;
    QLabel    *m_statusLabel      = nullptr;
};

} // namespace linscp::ui
