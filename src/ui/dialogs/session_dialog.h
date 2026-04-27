#pragma once
#include <QDialog>
#include "core/session/session_profile.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;

namespace linscp::ui::dialogs {

/// Основной диалог подключения — аналог диалога «Вход» в WinSCP.
/// Содержит только базовые поля: протокол, хост, порт, пользователь, пароль.
/// Кнопка «Ещё…» открывает AdvancedSessionDialog для остальных настроек.
class SessionDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionDialog(QWidget *parent = nullptr);
    explicit SessionDialog(const core::session::SessionProfile &profile,
                           QWidget *parent = nullptr);

    core::session::SessionProfile profile() const;

private slots:
    void onMoreClicked();
    void onProtocolChanged(int index);
    void onEncryptionChanged(int index);
    void onAuthMethodChanged(int index);
    void onBrowseKey();
    void onTestConnection();

private:
    void setupUi();
    void populate(const core::session::SessionProfile &profile);
    bool isWebDav() const;

    // Connection
    QComboBox   *m_protocol;
    QComboBox   *m_encryption;  ///< WebDAV: Без шифрования / TLS / Implicit TLS
    QLabel      *m_encryptionLabel = nullptr;
    QLineEdit   *m_host;
    QSpinBox    *m_port;
    QLineEdit   *m_username;
    QComboBox   *m_authMethod;
    QLabel      *m_authMethodLabel = nullptr;
    QLineEdit   *m_password;    ///< видим только при authMethod == Password
    QLineEdit   *m_keyPath;     ///< видим только при authMethod == PublicKey
    QPushButton *m_browseKey;

    QPushButton *m_testBtn;
    QLabel      *m_testStatus;

    // Хранит расширенные настройки, изменённые через AdvancedSessionDialog
    core::session::SessionProfile m_advancedData;
};

} // namespace linscp::ui::dialogs
