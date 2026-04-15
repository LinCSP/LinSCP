#include "app_settings.h"
#include <QSettings>
#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace linscp::core {

static QSettings s() { return QSettings("LinSCP", "LinSCP"); }

// ── Интерфейс ─────────────────────────────────────────────────────────────────

QString AppSettings::language()
{
    return s().value("language").toString();
}
void AppSettings::setLanguage(const QString &code)
{
    auto cfg = s(); cfg.setValue("language", code);
}

AppSettings::ThemeMode AppSettings::theme()
{
    return static_cast<ThemeMode>(
        s().value("interface/theme", int(ThemeMode::System)).toInt());
}
void AppSettings::setTheme(ThemeMode mode)
{
    auto cfg = s(); cfg.setValue("interface/theme", int(mode));
}

void AppSettings::applyTheme()
{
    const ThemeMode mode = theme();

    // Всегда использовать Fusion — он одинаково хорошо выглядит и с тёмной, и со светлой
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    if (mode == ThemeMode::System) {
        // Сбросить палитру — Qt подхватит системную тему
        QApplication::setPalette(QApplication::style()->standardPalette());
        return;
    }

    QPalette pal;
    if (mode == ThemeMode::Dark) {
        // Тёмная палитра в стиле WinSCP/VS Code Dark
        pal.setColor(QPalette::Window,          QColor(0x2b, 0x2b, 0x2b));
        pal.setColor(QPalette::WindowText,      QColor(0xd4, 0xd4, 0xd4));
        pal.setColor(QPalette::Base,            QColor(0x1e, 0x1e, 0x1e));
        pal.setColor(QPalette::AlternateBase,   QColor(0x25, 0x25, 0x25));
        pal.setColor(QPalette::Text,            QColor(0xd4, 0xd4, 0xd4));
        pal.setColor(QPalette::BrightText,      Qt::white);
        pal.setColor(QPalette::Button,          QColor(0x3c, 0x3c, 0x3c));
        pal.setColor(QPalette::ButtonText,      QColor(0xd4, 0xd4, 0xd4));
        pal.setColor(QPalette::Highlight,       QColor(0x26, 0x4f, 0x78));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::Link,            QColor(0x56, 0x9c, 0xd6));
        pal.setColor(QPalette::ToolTipBase,     QColor(0x1e, 0x1e, 0x1e));
        pal.setColor(QPalette::ToolTipText,     QColor(0xd4, 0xd4, 0xd4));
        pal.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
        // Disabled
        pal.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x66, 0x66, 0x66));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x66, 0x66, 0x66));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x66, 0x66, 0x66));
        // Mid / Shadow / Dark
        pal.setColor(QPalette::Mid,    QColor(0x50, 0x50, 0x50));
        pal.setColor(QPalette::Dark,   QColor(0x30, 0x30, 0x30));
        pal.setColor(QPalette::Shadow, QColor(0x10, 0x10, 0x10));
    } else {
        // Светлая — стандартная Fusion
        pal = QApplication::style()->standardPalette();
    }
    QApplication::setPalette(pal);
}

// ── Интеграция / Терминал ─────────────────────────────────────────────────────

AppSettings::TerminalMode AppSettings::terminalMode()
{
    return static_cast<TerminalMode>(
        s().value("terminal/mode", int(TerminalMode::AutoDetect)).toInt());
}
void AppSettings::setTerminalMode(TerminalMode mode)
{
    auto cfg = s(); cfg.setValue("terminal/mode", int(mode));
}

QString AppSettings::terminalCustomPath()
{
    return s().value("terminal/customPath").toString();
}
void AppSettings::setTerminalCustomPath(const QString &path)
{
    auto cfg = s(); cfg.setValue("terminal/customPath", path);
}

QString AppSettings::terminalCustomArgs()
{
    return s().value("terminal/customArgs",
                     "ssh -p %port% %user%@%host%").toString();
}
void AppSettings::setTerminalCustomArgs(const QString &args)
{
    auto cfg = s(); cfg.setValue("terminal/customArgs", args);
}

bool AppSettings::terminalAutoOpen()
{
    return s().value("terminal/autoOpen", false).toBool();
}
void AppSettings::setTerminalAutoOpen(bool v)
{
    auto cfg = s(); cfg.setValue("terminal/autoOpen", v);
}

// ── Передача файлов ───────────────────────────────────────────────────────────

int AppSettings::maxConcurrentTransfers()
{
    return s().value("transfer/maxConcurrent", 3).toInt();
}
void AppSettings::setMaxConcurrentTransfers(int n)
{
    auto cfg = s(); cfg.setValue("transfer/maxConcurrent", n);
}

// ── Вспомогательные ───────────────────────────────────────────────────────────

QString AppSettings::terminalBinaryForMode(TerminalMode mode)
{
    switch (mode) {
    case TerminalMode::PuTTY:         return "putty";
    case TerminalMode::Kitty:         return "kitty";
    case TerminalMode::Xterm:         return "xterm";
    case TerminalMode::Konsole:       return "konsole";
    case TerminalMode::GnomeTerminal: return "gnome-terminal";
    case TerminalMode::XfceTerminal:  return "xfce4-terminal";
    case TerminalMode::Alacritty:     return "alacritty";
    default: return {};
    }
}

QList<AppSettings::TerminalEntry> AppSettings::terminalEntries()
{
    return {
        { TerminalMode::AutoDetect,    "Auto-detect",       "" },
        { TerminalMode::PuTTY,         "PuTTY",             "putty" },
        { TerminalMode::Kitty,         "kitty",             "kitty" },
        { TerminalMode::Xterm,         "xterm",             "xterm" },
        { TerminalMode::Konsole,       "Konsole (KDE)",     "konsole" },
        { TerminalMode::GnomeTerminal, "GNOME Terminal",    "gnome-terminal" },
        { TerminalMode::XfceTerminal,  "Xfce Terminal",     "xfce4-terminal" },
        { TerminalMode::Alacritty,     "Alacritty",         "alacritty" },
        { TerminalMode::Custom,        "Custom…",           "" },
    };
}

} // namespace linscp::core
