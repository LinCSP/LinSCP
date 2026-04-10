#pragma once
#include <QDialog>
#include "core/ssh/ssh_session.h"

class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace linscp::ui::dialogs {

/// Диалог прогресса подключения — аналог TAuthenticateForm в WinSCP.
/// Показывает лог аутентификации и закрывается автоматически при успехе.
/// При ошибке остаётся открытым с кнопкой Close.
class AuthDialog : public QDialog {
    Q_OBJECT
public:
    explicit AuthDialog(core::ssh::SshSession *session, QWidget *parent = nullptr);

private slots:
    void onLogMessage(const QString &msg);
    void onConnected();
    void onError(const QString &msg);

private:
    void setupUi(const QString &host, quint16 port);

    QLabel        *m_titleLabel = nullptr;
    QPlainTextEdit *m_log       = nullptr;
    QPushButton   *m_cancelBtn  = nullptr;
};

} // namespace linscp::ui::dialogs
