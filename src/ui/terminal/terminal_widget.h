#pragma once
#include <QWidget>

class QLabel;
class QPushButton;
class QProcess;

namespace linscp::ui::terminal {

/// Виджет терминала — запускает системный SSH-терминал (PuTTY, kitty, xterm…)
/// в отдельном окне с параметрами текущего подключения.
///
/// Показывает плейсхолдер с кнопкой «Открыть терминал»;
/// при openShell() — ищет доступный эмулятор и запускает его автоматически.
class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    /// Передать параметры подключения (вызвать перед openShell)
    void setConnectionInfo(const QString &host, quint16 port,
                           const QString &username,
                           const QString &keyPath = {});

    /// Запустить внешний терминал. Безопасно вызывать повторно.
    void openShell();

    void closeShell();
    bool isShellOpen() const;

    void setTerminalFont(const QFont &) {}
    void setColorScheme(const QString &) {}

signals:
    void shellOpened();
    void shellClosed();

private slots:
    void onLaunchClicked();
    void onProcessFinished();

private:
    void setupUi();
    bool buildCommand(QString &program, QStringList &args) const;
    void updateStatus(const QString &text, bool isError = false);

    QString  m_host;
    quint16  m_port     = 22;
    QString  m_username;
    QString  m_keyPath;

    QPushButton *m_btnLaunch  = nullptr;
    QLabel      *m_statusLbl  = nullptr;
    QProcess    *m_process    = nullptr;
};

} // namespace linscp::ui::terminal
