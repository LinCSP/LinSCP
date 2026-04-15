#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QSettings>
#include "core/app_settings.h"
#include "ui/main_window.h"
#include "startup_log.h"

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

int main(int argc, char *argv[])
{
    installCrashHandler();
    slog("[linscp] main() entered");

    // ── Язык: читаем настройки ДО создания QApplication ──────────────────────
    // Qt6 фиксирует системный locale при создании QApplication. Поэтому если
    // пользователь выбрал язык отличный от системного, нужно переопределить
    // default locale заранее — иначе QFileSystemModel и другие Qt-классы
    // будут использовать системный язык независимо от наших QTranslator.
    {
        QSettings prefs("LinSCP", "LinSCP");
        if (prefs.contains("language")) {
            const QString saved = prefs.value("language").toString();
            // "en" или "" → принудительно C/English, иначе — выбранный язык
            if (saved.isEmpty() || saved == "en")
                QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
            else
                QLocale::setDefault(QLocale(saved));
        }
        // Если пользователь ничего не выбирал — системный locale остаётся.
    }

    QApplication app(argc, argv);
    slog("[linscp] QApplication created");

    app.setApplicationName("LinSCP");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("LinSCP");
    app.setOrganizationDomain("linscp.app");
    app.setWindowIcon(QIcon(":/icons/linscp.svg"));
    slog("[linscp] app metadata + icon set");

    // Применить тему оформления (System / Light / Dark) из настроек
    linscp::core::AppSettings::applyTheme();
    slog("[linscp] theme applied");

    // ── Переводы ──────────────────────────────────────────────────────────────

    const QSettings prefs("LinSCP", "LinSCP");
    const bool userChoseLang = prefs.contains("language");
    const QString saved = prefs.value("language").toString(); // "" / "en" = English

    const QLocale appLocale = QLocale(); // уже установлен setDefault выше

    // 1. Qt стандартные строки (кнопки диалогов, QFileSystemModel и т.д.)
    //    qtbase содержит реальные переводы (~200 КБ), qt — пустышка (~100 байт).
    static QTranslator qtTranslator;
    if (qtTranslator.load(appLocale, "qtbase", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    // 2. LinSCP: сначала явный выбор пользователя, потом системный locale
    static QTranslator appTranslator;
    bool loaded = false;
    if (userChoseLang) {
        loaded = loadAppTranslator(appTranslator, saved);
    } else {
        for (const QString &lang : QLocale::system().uiLanguages()) {
            const QString code = QLocale(lang).name().section('_', 0, 0); // "ru_RU" → "ru"
            if (loadAppTranslator(appTranslator, code)) { loaded = true; break; }
        }
    }
    if (loaded)
        app.installTranslator(&appTranslator);
    slog("[linscp] translators done");

    // Создать конфиг-директорию при первом запуске
    QDir().mkpath(QDir::homePath() + "/.config/linscp");
    slog("[linscp] config dir ready, constructing MainWindow...");

    linscp::ui::MainWindow w;
    slog("[linscp] MainWindow created, calling show()");
    w.show();
    slog("[linscp] show() called, entering event loop");

    return app.exec();
}
