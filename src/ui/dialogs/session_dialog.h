#pragma once
#include <QDialog>
#include "core/session/session_profile.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QTabWidget;
class QCheckBox;
class QLabel;

namespace linscp::ui::dialogs {

/// Диалог создания / редактирования профиля SSH-сессии.
/// Вкладки: General | Authentication | Advanced
class SessionDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionDialog(QWidget *parent = nullptr);
    explicit SessionDialog(const core::session::SessionProfile &profile,
                           QWidget *parent = nullptr);

    core::session::SessionProfile profile() const;

private slots:
    void onAuthMethodChanged(int index);
    void onBrowseKey();
    void onTestConnection();

private:
    void setupUi();
    void populate(const core::session::SessionProfile &profile);

    // General
    QLineEdit   *m_name;
    QLineEdit   *m_host;
    QSpinBox    *m_port;
    QLineEdit   *m_username;
    QLineEdit   *m_initialRemote;

    // Auth
    QComboBox   *m_authMethod;
    QLineEdit   *m_password;
    QLineEdit   *m_keyPath;
    QPushButton *m_browseKey;
    QCheckBox   *m_useAgent;

    // State
    QPushButton *m_testBtn;
    QLabel      *m_testStatus;
};

} // namespace linscp::ui::dialogs
