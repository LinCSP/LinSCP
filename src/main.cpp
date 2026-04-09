#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include "ui/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LinSCP");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("LinSCP");
    app.setOrganizationDomain("linscp.app");
    app.setWindowIcon(QIcon::fromTheme("network-server"));

    // ── Переводы ──────────────────────────────────────────────────────────────

    // 1. Qt стандартные строки (диалоги, кнопки и т.д.)
    static QTranslator qtTranslator;
    if (qtTranslator.load(QLocale::system(),
                          "qt", "_",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // 2. LinSCP строки (:/i18n/linscp_<locale>.qm)
    static QTranslator appTranslator;
    const QStringList langs = QLocale::system().uiLanguages();
    for (const QString &lang : langs) {
        const QString base = "linscp_" + QLocale(lang).name();
        if (appTranslator.load(base, ":/i18n")) {
            app.installTranslator(&appTranslator);
            break;
        }
        // Попробовать только код языка без региона (ru_RU → ru)
        const QString shortBase = "linscp_" + QLocale(lang).name().section('_', 0, 0);
        if (appTranslator.load(shortBase, ":/i18n")) {
            app.installTranslator(&appTranslator);
            break;
        }
    }

    // Создать конфиг-директорию при первом запуске
    QDir().mkpath(QDir::homePath() + "/.config/linscp");

    linscp::ui::MainWindow w;
    w.show();

    return app.exec();
}
