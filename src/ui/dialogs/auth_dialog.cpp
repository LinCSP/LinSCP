#include "auth_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFont>

namespace linscp::ui::dialogs {

AuthDialog::AuthDialog(core::ssh::SshSession *session, QWidget *parent)
    : QDialog(parent)
{
    setupUi(session->host(), session->port());

    // Подписываемся на сигналы SshSession (AutoConnection — безопасно из воркера)
    connect(session, &core::ssh::SshSession::logMessage,
            this, &AuthDialog::onLogMessage);
    connect(session, &core::ssh::SshSession::connected,
            this, &AuthDialog::onConnected);
    connect(session, &core::ssh::SshSession::errorOccurred,
            this, &AuthDialog::onError);
}

void AuthDialog::setupUi(const QString &host, quint16 port)
{
    setWindowTitle(tr("Connecting to %1").arg(host));
    setMinimumWidth(460);
    setMinimumHeight(240);

    auto *layout = new QVBoxLayout(this);

    m_titleLabel = new QLabel(
        tr("<b>Connecting to %1:%2</b>").arg(host).arg(port), this);
    layout->addWidget(m_titleLabel);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(200);
    QFont f("monospace");
    f.setPointSize(9);
    m_log->setFont(f);
    layout->addWidget(m_log, 1);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    btnRow->addWidget(m_cancelBtn);
    layout->addLayout(btnRow);

    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void AuthDialog::onLogMessage(const QString &msg)
{
    m_log->appendPlainText(msg);
}

void AuthDialog::onConnected()
{
    // Успешно — закрываем диалог автоматически
    accept();
}

void AuthDialog::onError(const QString &msg)
{
    m_log->appendPlainText(tr("Error: %1").arg(msg));
    m_cancelBtn->setText(tr("Close"));
    // Остаёмся открытыми — пользователь читает ошибку
}

} // namespace linscp::ui::dialogs
