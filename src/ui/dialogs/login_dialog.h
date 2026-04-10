#pragma once
#include <QDialog>
#include "core/session/session_profile.h"
#include "core/session/session_store.h"

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class QGroupBox;
class QMenu;
class QToolButton;

namespace linscp::ui::dialogs {

/// Главный диалог входа — аналог TLoginDialog (WinSCP).
/// Открывается при «Новая вкладка» и содержит:
///   – дерево сохранённых сессий слева
///   – форму подключения справа
///   – кнопки Инструменты / Управление / Войти / Закрыть / Справка внизу
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(core::session::SessionStore *store,
                         QWidget *parent = nullptr);

    /// Профиль, с которым нужно соединяться (после Войти)
    core::session::SessionProfile selectedProfile() const { return m_selectedProfile; }

private slots:
    void onTreeSelectionChanged();
    void onLogin();
    void onSave();
    void onAdvanced();
    void onManageMenu();
    void onToolsMenu();
    void onAuthMethodChanged(int index);
    void onBrowseKey();
    // Управление
    void onNewFolder();
    void onDeleteSession();
    void onRenameSession();
    void onDuplicateSession();
    // Инструменты
    void onImportWinScp();
    void onImportSessions();
    void onExportSessions();

private:
    void setupUi();
    void buildTree();

    /// Найти или создать узел-папку для пути "A-media/Cobalt-pro"
    QTreeWidgetItem *ensureFolder(const QString &groupPath);

    /// Папка, в которой находится текущий выбранный элемент (или nullptr)
    QTreeWidgetItem *currentFolderItem() const;

    void showNewConnectionForm();
    void showSessionForm(const core::session::SessionProfile &p);

    /// Собрать данные из формы в профиль
    core::session::SessionProfile collectProfile() const;

    /// Обновить состояние кнопок (Войти, Управление, Изменить)
    void updateButtonStates();

    core::session::SessionStore     *m_store;
    core::session::SessionProfile    m_selectedProfile;
    bool                             m_isNewConnection = true;

    // ── Left panel ────────────────────────────────────────────────────────────
    QTreeWidget  *m_tree;

    // ── Right panel ───────────────────────────────────────────────────────────
    QGroupBox    *m_connGroup;

    QComboBox    *m_protocol;
    QLineEdit    *m_host;
    QSpinBox     *m_port;
    QLineEdit    *m_username;
    QComboBox    *m_authMethod;
    QLineEdit    *m_password;
    QLineEdit    *m_keyPath;
    QPushButton  *m_browseKey;
    QPushButton  *m_saveBtn;     ///< «Сохранить» (новое) / «Изменить» (существующее)
    QPushButton  *m_advancedBtn; ///< «Ещё…»

    QGroupBox    *m_noteGroup;
    QPlainTextEdit *m_noteMemo;

    // ── Bottom buttons ────────────────────────────────────────────────────────
    QToolButton  *m_toolsBtn;
    QToolButton  *m_manageBtn;
    QPushButton  *m_loginBtn;
    QPushButton  *m_closeBtn;

    QMenu *m_manageMenu;
    QMenu *m_toolsMenu;

    // Действия меню Управление (нужны для enable/disable)
    QAction *m_actDelete;
    QAction *m_actRename;
    QAction *m_actDuplicate;
};

} // namespace linscp::ui::dialogs
