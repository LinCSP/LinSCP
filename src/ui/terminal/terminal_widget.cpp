#include "terminal_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProcess>
#include <QStandardPaths>
#include <QFont>

namespace linscp::ui::terminal {

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

TerminalWidget::~TerminalWidget()
{
    closeShell();
}

void TerminalWidget::setupUi()
{
    setStyleSheet("background:#1e1e1e;");

    auto *root = new QVBoxLayout(this);
    root->setAlignment(Qt::AlignCenter);
    root->setSpacing(14);

    auto *ico = new QLabel("⌨", this);
    ico->setAlignment(Qt::AlignCenter);
    ico->setStyleSheet("font-size:40px; background:transparent;");

    auto *title = new QLabel(tr("SSH Terminal"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color:#ccc; font-size:15px; font-weight:bold; background:transparent;");

    m_statusLbl = new QLabel(tr("Set a connection to enable the terminal."), this);
    m_statusLbl->setAlignment(Qt::AlignCenter);
    m_statusLbl->setStyleSheet("color:#777; font-size:11px; background:transparent;");
    m_statusLbl->setWordWrap(true);

    m_btnLaunch = new QPushButton(tr("Open Terminal"), this);
    m_btnLaunch->setEnabled(false);
    m_btnLaunch->setFixedWidth(180);
    m_btnLaunch->setStyleSheet(
        "QPushButton { background:#0e639c; color:#fff; border:none;"
        "  padding:8px 0; border-radius:4px; font-size:13px; }"
        "QPushButton:hover    { background:#1177bb; }"
        "QPushButton:pressed  { background:#0a4f80; }"
        "QPushButton:disabled { background:#333; color:#555; }");

    root->addWidget(ico);
    root->addWidget(title);
    root->addWidget(m_statusLbl);
    root->addSpacing(4);
    root->addWidget(m_btnLaunch, 0, Qt::AlignHCenter);

    connect(m_btnLaunch, &QPushButton::clicked, this, &TerminalWidget::onLaunchClicked);
}

// ── Параметры подключения ─────────────────────────────────────────────────────

void TerminalWidget::setConnectionInfo(const QString &host, quint16 port,
                                        const QString &username,
                                        const QString &keyPath)
{
    m_host     = host;
    m_port     = port;
    m_username = username;
    m_keyPath  = keyPath;

    if (host.isEmpty()) {
        m_btnLaunch->setEnabled(false);
        updateStatus(tr("Set a connection to enable the terminal."));
    } else {
        QString prog;
        QStringList dummy;
        const bool canLaunch = buildCommand(prog, dummy);
        m_btnLaunch->setEnabled(canLaunch);
        if (canLaunch)
            updateStatus(tr("Ready: %1@%2:%3  ·  via %4")
                             .arg(username, host).arg(port).arg(prog));
        else
            updateStatus(tr("No terminal emulator found.\n"
                            "Install putty, kitty, xterm, konsole or gnome-terminal."),
                         true);
    }
}

// ── Управление процессом ──────────────────────────────────────────────────────

void TerminalWidget::openShell()
{
    if (m_host.isEmpty() || isShellOpen()) return;
    onLaunchClicked();
}

void TerminalWidget::closeShell()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        m_process->waitForFinished(1000);
    }
}

bool TerminalWidget::isShellOpen() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void TerminalWidget::onLaunchClicked()
{
    if (m_host.isEmpty()) return;

    QString prog;
    QStringList args;
    if (!buildCommand(prog, args)) {
        updateStatus(tr("No terminal emulator found.\n"
                        "Install putty, kitty, xterm, konsole or gnome-terminal."), true);
        return;
    }

    delete m_process;
    m_process = new QProcess(this);
    connect(m_process, &QProcess::started, this, [this, prog]() {
        updateStatus(tr("Running in %1…  (close the terminal window to stop)").arg(prog));
        m_btnLaunch->setEnabled(false);
        emit shellOpened();
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &TerminalWidget::onProcessFinished);

    m_process->start(prog, args);
}

void TerminalWidget::onProcessFinished()
{
    m_btnLaunch->setEnabled(!m_host.isEmpty());
    updateStatus(tr("Ready: %1@%2:%3").arg(m_username, m_host).arg(m_port));
    emit shellClosed();
}

// ── Поиск терминала ───────────────────────────────────────────────────────────

bool TerminalWidget::buildCommand(QString &program, QStringList &args) const
{
    // Базовые аргументы ssh
    QStringList ssh;
    ssh << "-p" << QString::number(m_port)
        << "-o" << "StrictHostKeyChecking=ask";
    if (!m_keyPath.isEmpty())
        ssh << "-i" << m_keyPath;
    ssh << QString("%1@%2").arg(m_username, m_host);

    // PuTTY — сам умеет SSH без внешнего ssh
    const QString putty = QStandardPaths::findExecutable("putty");
    if (!putty.isEmpty()) {
        program = putty;
        args = { "-ssh", "-P", QString::number(m_port), "-l", m_username };
        if (!m_keyPath.isEmpty()) args << "-i" << m_keyPath;
        args << m_host;
        return true;
    }

    // Функция-помощник: ищет терминал и формирует аргументы
    struct T { const char *bin; QStringList prefix; };
    const QList<T> candidates = {
        { "kitty",          { "ssh" }           },
        { "xterm",          { "-e", "ssh" }     },
        { "konsole",        { "-e", "ssh" }     },
        { "xfce4-terminal", { "-e", "ssh" }     },   // аргументы одной строкой ниже
        { "gnome-terminal", { "--", "ssh" }     },
        { "lxterminal",     { "-e", "ssh" }     },
        { "mate-terminal",  { "-e", "ssh" }     },
        { "tilix",          { "-e", "ssh" }     },
        { "alacritty",      { "-e", "ssh" }     },
    };

    for (const auto &c : candidates) {
        const QString bin = QStandardPaths::findExecutable(c.bin);
        if (bin.isEmpty()) continue;
        program = bin;
        if (qstrcmp(c.bin, "xfce4-terminal") == 0) {
            // xfce4-terminal не поддерживает список аргументов через -e,
            // принимает строку целиком
            args = { "-e", "ssh " + ssh.join(' ') };
        } else {
            args = c.prefix + ssh;
        }
        return true;
    }

    return false;
}

void TerminalWidget::updateStatus(const QString &text, bool isError)
{
    m_statusLbl->setText(text);
    m_statusLbl->setStyleSheet(
        isError ? "color:#f44; font-size:11px; background:transparent;"
                : "color:#777; font-size:11px; background:transparent;");
}

} // namespace linscp::ui::terminal
