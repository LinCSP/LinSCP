#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QPalette>
#include "ui/main_window.h"

/// Загрузить переводчик LinSCP для заданного кода языка (например "ru").
/// Пустая строка — английский (нет файла перевода, используются source-строки).
static bool loadAppTranslator(QTranslator &t, const QString &langCode)
{
    if (langCode.isEmpty()) return false;
    const QString name = "linscp_" + langCode;
    // 1. Встроенный ресурс :/i18n/linscp_ru.qm
    if (t.load(name, ":/i18n"))                                    return true;
    // 2. Рядом с бинарником (сборочная директория / portable)
    if (t.load(name, QCoreApplication::applicationDirPath()))      return true;
    // 3. Системный каталог переводов (/usr/share/linscp/translations и т.п.)
    if (t.load(name, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) return true;
    return false;
}

// Startup log — enabled only in LINSCP_STARTLOG builds (debug workflow).
// Writes to a file so it works regardless of subsystem or NDEBUG.
#ifdef LINSCP_STARTLOG
#include <cstdio>
static FILE *g_log = nullptr;
static void slog(const char *msg)
{
    if (!g_log) g_log = fopen("linscp_startup.log", "w");
    if (!g_log) return;
    fputs(msg, g_log);
    fputc('\n', g_log);
    fflush(g_log);
}
#else
static inline void slog(const char *) {}
#endif

int main(int argc, char *argv[])
{
    slog("[linscp] main() entered");
    QApplication app(argc, argv);
    slog("[linscp] QApplication created");
    app.setApplicationName("LinSCP");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("LinSCP");
    app.setOrganizationDomain("linscp.app");
    app.setWindowIcon(QIcon::fromTheme("network-server",
                      app.style()->standardIcon(QStyle::SP_DriveNetIcon)));

    // Fusion — чистый, современный стиль без зависимости от темы рабочего стола
    app.setStyle(QStyleFactory::create("Fusion"));

    // Акцентный цвет — тёмно-синий
    QPalette pal = app.palette();
    pal.setColor(QPalette::Highlight,       QColor(0x0D6EFD));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(pal);

    // ── Переводы ──────────────────────────────────────────────────────────────

    // 1. Qt стандартные строки (кнопки диалогов и т.д.)
    static QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::system(), "qt", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    // 2. LinSCP: сначала явный выбор пользователя, потом системный locale
    static QTranslator appTranslator;
    const QSettings prefs("LinSCP", "LinSCP");
    const bool userChoseLang = prefs.contains("language");
    const QString saved = prefs.value("language").toString(); // "" / "en" = English

    bool loaded = false;
    if (userChoseLang) {
        // Пользователь явно выбрал язык — уважаем выбор, не делаем автодетект.
        // "en" или "" означает English: переводчик не нужен.
        loaded = loadAppTranslator(appTranslator, saved);
    } else {
        // Язык не задан — автодетект по системному locale
        for (const QString &lang : QLocale::system().uiLanguages()) {
            const QString code = QLocale(lang).name().section('_', 0, 0); // "ru_RU" → "ru"
            if (loadAppTranslator(appTranslator, code)) { loaded = true; break; }
        }
    }
    if (loaded)
        app.installTranslator(&appTranslator);

    // Создать конфиг-директорию при первом запуске
    QDir().mkpath(QDir::homePath() + "/.config/linscp");

    linscp::ui::MainWindow w;
    slog("[linscp] MainWindow created, calling show()");
    w.show();

    return app.exec();
}
