#include "svg_icon.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>

namespace linscp {

QIcon svgIcon(const QString &qrcName, const QByteArray &customFill)
{
    QFile f(QStringLiteral(":/icons/") + qrcName + QStringLiteral(".svg"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = f.readAll();

    // Strip XML comments — Qt SVG parser may choke on <!--! ... -->
    for (int a = data.indexOf("<!--"); a != -1; a = data.indexOf("<!--", a)) {
        int b = data.indexOf("-->", a);
        if (b == -1) break;
        data.remove(a, b - a + 3);
    }

    QByteArray fill;
    if (!customFill.isEmpty()) {
        fill = QByteArrayLiteral("fill=\"") + customFill + QByteArrayLiteral("\"");
    } else {
        // Pick icon colour based on button background brightness
        const QPalette pal = QApplication::palette();
        const int lum = pal.color(QPalette::Active, QPalette::Button).lightness();
        fill = (lum < 128)
            ? QByteArrayLiteral("fill=\"#c0c4cc\"")
            : QByteArrayLiteral("fill=\"#6c757d\"");
    }

    data.replace(QByteArrayLiteral("<path "), QByteArrayLiteral("<path ") + fill + ' ');

    QSvgRenderer renderer(data);
    if (!renderer.isValid())
        return {};

    QIcon icon;
    for (int sz : {16, 20, 24, 32}) {
        QPixmap pm(sz, sz);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            renderer.render(&p, QRectF(0, 0, sz, sz));
        }
        icon.addPixmap(pm, QIcon::Normal);

        QPixmap pmDis(sz, sz);
        pmDis.fill(Qt::transparent);
        {
            QPainter p(&pmDis);
            p.setOpacity(0.35);
            renderer.render(&p, QRectF(0, 0, sz, sz));
        }
        icon.addPixmap(pmDis, QIcon::Disabled);
    }
    return icon;
}

QIcon svgFolderIcon(const QString &qrcName)
{
    const QPalette pal = QApplication::palette();
    const int lum = pal.color(QPalette::Active, QPalette::Button).lightness();
    const QByteArray fill = (lum < 128)
        ? QByteArrayLiteral("#dde1ea")
        : QByteArrayLiteral("#9aa3b0");
    return svgIcon(qrcName, fill);
}

} // namespace linscp
