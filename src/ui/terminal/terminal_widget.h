#pragma once
#include <QWidget>
#include <memory>

class QTimer;
class QFont;

namespace linscp::core::ssh {
class SshSession;
class SshChannel;
}

#ifdef HAVE_QTERMWIDGET
class QTermWidget;
#else
class QPlainTextEdit;
class QLineEdit;
#endif

namespace linscp::ui::terminal {

/// Виджет встроенного терминала.
///
/// Открывает Shell-канал поверх уже подключённой SshSession и эмулирует
/// терминал VT100/xterm-256color. При наличии qtermwidget использует его,
/// иначе — простой fallback (QPlainTextEdit + поле ввода команд).
///
/// Использование:
///   auto *term = new TerminalWidget(session, this);
///   term->openShell();   // запрашивает pty + shell
class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(core::ssh::SshSession *session,
                            QWidget *parent = nullptr);
    ~TerminalWidget() override;

    /// Открыть shell-канал. Вызвать после того, как SshSession::connected().
    void openShell();

    /// Закрыть канал (не сессию).
    void closeShell();

    bool isShellOpen() const;

    // ── Внешний вид ──────────────────────────────────────────────────────────
    void setTerminalFont(const QFont &font);
    void setColorScheme(const QString &name);   ///< напр. "Linux", "Solarized"

signals:
    void shellOpened();
    void shellClosed();
    void titleChanged(const QString &title);    ///< от escape-последовательности

private slots:
    void onReadReady();     ///< QTimer poll — читать данные из ssh_channel

#ifndef HAVE_QTERMWIDGET
    void onCommandEntered(); ///< fallback: пользователь нажал Enter в строке ввода
#endif

private:
    void setupUi();

    core::ssh::SshSession *m_session;
    core::ssh::SshChannel *m_channel = nullptr;

    QTimer *m_readTimer = nullptr;  ///< 20 мс — опрос ssh_channel_read_nonblocking

#ifdef HAVE_QTERMWIDGET
    QTermWidget *m_term = nullptr;
#else
    QPlainTextEdit *m_output = nullptr;
    QLineEdit      *m_input  = nullptr;
#endif
};

} // namespace linscp::ui::terminal
