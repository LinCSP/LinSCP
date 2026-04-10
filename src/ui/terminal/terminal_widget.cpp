#include "terminal_widget.h"
#include "core/ssh/ssh_session.h"
#include "core/ssh/ssh_channel.h"

#include <QVBoxLayout>
#include <QTimer>
#include <QFont>
#include <QFontDatabase>

#ifdef HAVE_QTERMWIDGET
#  include <qtermwidget5/qtermwidget.h>
#endif

#ifndef HAVE_QTERMWIDGET
#  include <QPlainTextEdit>
#  include <QLineEdit>
#  include <QLabel>
#endif

#include <libssh/libssh.h>

namespace linscp::ui::terminal {

using namespace core::ssh;

static constexpr int kReadIntervalMs = 20;
static constexpr int kReadBufSize    = 4096;

// ─────────────────────────────────────────────────────────────────────────────

TerminalWidget::TerminalWidget(SshSession *session, QWidget *parent)
    : QWidget(parent)
    , m_session(session)
{
    setupUi();
}

TerminalWidget::~TerminalWidget()
{
    closeShell();
}

// ── Построение UI ─────────────────────────────────────────────────────────────

void TerminalWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

#ifdef HAVE_QTERMWIDGET
    m_term = new QTermWidget(0, this);   // 0 = не запускать локальный shell

    // Шрифт monospace
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(10);
    m_term->setTerminalFont(f);
    m_term->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_term->setColorScheme("Linux");

    layout->addWidget(m_term);

    // Данные от qtermwidget (пользователь набирает) → SSH-канал
    connect(m_term, &QTermWidget::sendData, this,
            [this](const char *data, int len) {
        if (m_channel && m_channel->isOpen())
            ssh_channel_write(m_channel->handle(), data, static_cast<uint32_t>(len));
    });
#else
    // Fallback: QPlainTextEdit (только вывод) + QLineEdit (ввод)
    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);

    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(10);
    m_output->setFont(f);
    m_output->setStyleSheet("background:#1e1e1e; color:#d4d4d4;");

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Enter command…"));
    m_input->setFont(f);

    layout->addWidget(m_output, 1);
    layout->addWidget(new QLabel(tr("⚠ Full terminal requires qtermwidget. "
                                   "Command-line mode active."), this));
    layout->addWidget(m_input);

    connect(m_input, &QLineEdit::returnPressed,
            this, &TerminalWidget::onCommandEntered);
#endif

    // Таймер опроса SSH-канала
    m_readTimer = new QTimer(this);
    m_readTimer->setInterval(kReadIntervalMs);
    connect(m_readTimer, &QTimer::timeout, this, &TerminalWidget::onReadReady);
}

// ── Управление shell ──────────────────────────────────────────────────────────

void TerminalWidget::openShell()
{
    if (m_channel && m_channel->isOpen()) return;
    if (!m_session || m_session->state() != core::ssh::SessionState::Connected) return;

    m_channel = m_session->openChannel(core::ssh::ChannelType::Shell);
    if (!m_channel) return;

    ssh_channel ch = m_channel->handle();

    // Запросить pseudo-terminal (pty)
#ifdef HAVE_QTERMWIDGET
    const QSize sz = m_term->terminalSizeHint();
    ssh_channel_request_pty_size(ch, "xterm-256color", sz.width(), sz.height());
#else
    ssh_channel_request_pty_size(ch, "xterm", 80, 24);
#endif

    if (ssh_channel_request_shell(ch) != SSH_OK) {
        m_channel->close();
        m_channel = nullptr;
        return;
    }

#ifdef HAVE_QTERMWIDGET
    // Передать управление размером в qtermwidget → SSH resize
    connect(m_term, &QTermWidget::termGetFocus, this, [this]() {
        if (!m_channel || !m_channel->isOpen()) return;
        const QSize sz = m_term->terminalSizeHint();
        ssh_channel_change_pty_size(m_channel->handle(), sz.width(), sz.height());
    });
#endif

    connect(m_channel, &SshChannel::closed, this, [this]() {
        m_readTimer->stop();
        emit shellClosed();
    });

    m_readTimer->start();
    emit shellOpened();
}

void TerminalWidget::closeShell()
{
    m_readTimer->stop();
    if (m_channel) {
        m_channel->close();
        m_channel = nullptr;
    }
}

bool TerminalWidget::isShellOpen() const
{
    return m_channel && m_channel->isOpen();
}

// ── Чтение данных из SSH-канала ───────────────────────────────────────────────

void TerminalWidget::onReadReady()
{
    if (!m_channel || !m_channel->isOpen()) {
        m_readTimer->stop();
        return;
    }

    ssh_channel ch = m_channel->handle();

    // Проверяем EOF
    if (ssh_channel_is_eof(ch)) {
        m_readTimer->stop();
        emit shellClosed();
        return;
    }

    char buf[kReadBufSize];
    int  n = ssh_channel_read_nonblocking(ch, buf, sizeof(buf), 0);
    if (n > 0) {
        const QByteArray data(buf, n);
#ifdef HAVE_QTERMWIDGET
        m_term->receiveData(data.constData(), data.size());
#else
        // Fallback: просто вставляем текст (без VT-интерпретации)
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(QString::fromLocal8Bit(data));
        m_output->ensureCursorVisible();
#endif
    }

    // Также stderr
    n = ssh_channel_read_nonblocking(ch, buf, sizeof(buf), 1 /* is_stderr */);
    if (n > 0) {
        const QByteArray err(buf, n);
#ifdef HAVE_QTERMWIDGET
        m_term->receiveData(err.constData(), err.size());
#else
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(QString::fromLocal8Bit(err));
        m_output->ensureCursorVisible();
#endif
    }
}

// ── Внешний вид ───────────────────────────────────────────────────────────────

void TerminalWidget::setTerminalFont(const QFont &font)
{
#ifdef HAVE_QTERMWIDGET
    m_term->setTerminalFont(font);
#else
    m_output->setFont(font);
    if (m_input) m_input->setFont(font);
#endif
}

void TerminalWidget::setColorScheme(const QString &name)
{
#ifdef HAVE_QTERMWIDGET
    m_term->setColorScheme(name);
#else
    Q_UNUSED(name)
#endif
}

// ── Fallback: ввод команды ────────────────────────────────────────────────────

#ifndef HAVE_QTERMWIDGET
void TerminalWidget::onCommandEntered()
{
    if (!m_channel || !m_channel->isOpen()) return;

    const QByteArray cmd = m_input->text().toLocal8Bit() + '\n';
    ssh_channel_write(m_channel->handle(), cmd.constData(),
                      static_cast<uint32_t>(cmd.size()));

    // Эхо в output
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(m_input->text() + '\n');
    m_output->ensureCursorVisible();

    m_input->clear();
}
#endif

} // namespace linscp::ui::terminal
