#pragma once
#include <QString>
#include <QStringList>

namespace linscp::core {

/// Центральное хранилище настроек приложения (обёртка над QSettings).
/// Все методы статические — используется как синглтон через QSettings("LinSCP","LinSCP").
class AppSettings {
public:
    AppSettings() = delete;

    // ── Интерфейс ─────────────────────────────────────────────────────────────
    static QString language();
    static void    setLanguage(const QString &code);

    enum class ThemeMode {
        System = 0,  ///< следовать системной теме
        Light  = 1,  ///< принудительно светлая
        Dark   = 2,  ///< принудительно тёмная
    };

    static ThemeMode theme();
    static void      setTheme(ThemeMode mode);
    /// Применить текущую тему к QApplication (вызывать при старте и при смене темы)
    static void      applyTheme();

    // ── Интеграция / Терминал ─────────────────────────────────────────────────

    /// Режим выбора терминала
    enum class TerminalMode {
        AutoDetect = 0,  ///< автоматически искать putty/kitty/xterm/…
        PuTTY      = 1,
        Kitty      = 2,
        Xterm      = 3,
        Konsole    = 4,
        GnomeTerminal = 5,
        XfceTerminal  = 6,
        Alacritty  = 7,
        Custom     = 8,  ///< путь + шаблон аргументов задаётся вручную
    };

    static TerminalMode terminalMode();
    static void         setTerminalMode(TerminalMode mode);

    /// Путь к кастомному терминалу (используется при mode == Custom)
    static QString terminalCustomPath();
    static void    setTerminalCustomPath(const QString &path);

    /// Шаблон аргументов для кастомного терминала.
    /// Плейсхолдеры: %host%, %port%, %user%, %key%
    /// Пример: "ssh -p %port% %user%@%host%"
    static QString terminalCustomArgs();
    static void    setTerminalCustomArgs(const QString &args);

    /// Автоматически открывать терминал при подключении
    static bool terminalAutoOpen();
    static void setTerminalAutoOpen(bool v);

    // ── Передача файлов ───────────────────────────────────────────────────────
    static int  maxConcurrentTransfers();
    static void setMaxConcurrentTransfers(int n);

    // ── Вспомогательные ───────────────────────────────────────────────────────

    /// Имя бинарника для заданного режима (пустая строка для AutoDetect/Custom)
    static QString terminalBinaryForMode(TerminalMode mode);

    /// Список описаний для ComboBox в диалоге
    struct TerminalEntry {
        TerminalMode mode;
        QString      displayName;
        QString      binary;        ///< "" = auto/custom
    };
    static QList<TerminalEntry> terminalEntries();
};

} // namespace linscp::core
