#include "app_settings.h"
#include <QSettings>

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
