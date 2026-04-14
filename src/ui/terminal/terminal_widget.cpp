#include "terminal_widget.h"

#include "core/app_settings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
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
                                        const QString &keyPath,
                                        const QString &password)
{
    m_host     = host;
    m_port     = port;
    m_username = username;
    m_keyPath  = keyPath;
    m_password = password;

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

    // Парольная аутентификация без ключа: вставить sshpass перед ssh.
    // putty обрабатывает пароль самостоятельно через -pw (buildCommand).
    if (!m_password.isEmpty() && m_keyPath.isEmpty())
        injectSshpass(args);

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

bool TerminalWidget::injectSshpass(QStringList &args)
{
    const QString sshpassBin = QStandardPaths::findExecutable(QStringLiteral("sshpass"));
    if (sshpassBin.isEmpty()) {
        // sshpass не установлен — SSH сам запросит пароль в терминале
        return false;
    }

    // Удалить предыдущий файл, если остался
    if (!m_passfilePath.isEmpty()) {
        QFile::remove(m_passfilePath);
        m_passfilePath.clear();
    }

    const QString pfPath = QDir::tempPath()
        + QStringLiteral("/linscp_pass_%1")
              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));

    QFile pf(pfPath);
    if (!pf.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    pf.write((m_password + '\n').toUtf8());
    pf.close();
    QFile::setPermissions(pfPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    m_passfilePath = pfPath;

    // Вставить "sshpass -f FILE" перед "ssh" в аргументах.
    // Большинство эмуляторов: args = [..., "ssh", ssh-flags…]
    // xfce4-terminal:        args = ["-e", "ssh -p …"]  (вся команда одной строкой)
    const int sshIdx = args.indexOf(QStringLiteral("ssh"));
    if (sshIdx >= 0) {
        args.insert(sshIdx, pfPath);
        args.insert(sshIdx, QStringLiteral("-f"));
        args.insert(sshIdx, sshpassBin);
    } else {
        // xfce4-terminal: одиночная строка после -e
        const int eIdx = args.indexOf(QStringLiteral("-e"));
        if (eIdx >= 0 && eIdx + 1 < args.size()) {
            args[eIdx + 1] = sshpassBin + QStringLiteral(" -f ") + pfPath
                             + QStringLiteral(" ") + args[eIdx + 1];
        }
    }

    // Удалить файл через 30 с — sshpass читает его в момент аутентификации
    QTimer::singleShot(30'000, this, [this]() {
        if (!m_passfilePath.isEmpty()) {
            QFile::remove(m_passfilePath);
            m_passfilePath.clear();
        }
    });

    return true;
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
    using Mode = core::AppSettings::TerminalMode;
    const Mode mode = core::AppSettings::terminalMode();

    // Базовые ssh-аргументы (используются всеми кроме putty и custom)
    auto makeSshArgs = [this]() -> QStringList {
        QStringList a;
        a << "-p" << QString::number(m_port)
          << "-o" << "StrictHostKeyChecking=ask";
        if (!m_keyPath.isEmpty()) a << "-i" << m_keyPath;
        a << QString("%1@%2").arg(m_username, m_host);
        return a;
    };

    // Сборка команды для конкретного бинарника
    auto buildFor = [&](const QString &bin) -> bool {
        const QString path = QStandardPaths::findExecutable(bin);
        if (path.isEmpty()) return false;
        program = path;
        if (bin == "putty") {
            // putty имеет собственный механизм аутентификации;
            // -pw передаёт пароль напрямую (видно в ps, но putty иначе не умеет)
            args = { "-ssh", "-P", QString::number(m_port), "-l", m_username };
            if (!m_keyPath.isEmpty()) args << "-i" << m_keyPath;
            if (!m_password.isEmpty()) args << "-pw" << m_password;
            args << m_host;
        } else if (bin == "kitty") {
            // kitty: программа передаётся как аргументы напрямую (без -e)
            args = QStringList{ "ssh" } + makeSshArgs();
        } else if (bin == "alacritty") {
            args = QStringList{ "-e", "ssh" } + makeSshArgs();
        } else if (bin == "xfce4-terminal") {
            // xfce4-terminal -e принимает команду одной строкой
            args = { "-e", "ssh " + makeSshArgs().join(' ') };
        } else if (bin == "gnome-terminal") {
            args = QStringList{ "--", "ssh" } + makeSshArgs();
        } else if (bin == "konsole") {
            // --nofork: не передавать вкладку существующему процессу konsole.
            // Без него konsole-лаунчер сразу выходит → QProcess теряет трекинг,
            // temp-файл пароля удаляется до того как sshpass успевает его прочитать.
            args = QStringList{ "--nofork", "-e", "ssh" } + makeSshArgs();
        } else {
            // xterm, lxterminal, mate-terminal, tilix…
            args = QStringList{ "-e", "ssh" } + makeSshArgs();
        }
        return true;
    };

    // Custom — путь + шаблон аргументов из настроек
    if (mode == Mode::Custom) {
        const QString customPath = core::AppSettings::terminalCustomPath();
        if (customPath.isEmpty()) return false;
        program = customPath;
        QString tmpl = core::AppSettings::terminalCustomArgs();
        tmpl.replace("%host%", m_host);
        tmpl.replace("%port%", QString::number(m_port));
        tmpl.replace("%user%", m_username);
        tmpl.replace("%key%",  m_keyPath);
        args = tmpl.split(' ', Qt::SkipEmptyParts);
        return true;
    }

    // Конкретный выбор
    if (mode != Mode::AutoDetect) {
        const QString bin = core::AppSettings::terminalBinaryForMode(mode);
        return buildFor(bin);
    }

    // AutoDetect — перебираем по приоритету
    for (const char *bin : { "putty","kitty","xterm","konsole",
                              "gnome-terminal","xfce4-terminal",
                              "alacritty","lxterminal","mate-terminal","tilix" }) {
        if (buildFor(bin)) return true;
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
