#include <QApplication>
#include <QIcon>
#include <QDir>
#include "ui/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LinSCP");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("LinSCP");
    app.setOrganizationDomain("linscp.app");
    app.setWindowIcon(QIcon::fromTheme("network-server"));

    // Создать конфиг-директорию при первом запуске
    QDir().mkpath(QDir::homePath() + "/.config/linscp");

    linscp::ui::MainWindow w;
    w.show();

    return app.exec();
}
