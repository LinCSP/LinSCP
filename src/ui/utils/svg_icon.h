#pragma once

#include <QIcon>
#include <QString>

namespace linscp {

// Returns a QIcon backed by a Font Awesome SVG from the Qt resource system.
// The icon is rendered at the requested size and colorized automatically from
// the application palette (QPalette::WindowText for Normal/Active,
// QPalette::Disabled/WindowText for Disabled).  Palette changes take effect
// on the next paint call — no restart needed.
//
// qrcName  — bare icon name without prefix or extension, e.g. "trash".
//            The path ":/icons/<name>.svg" is used internally.
// qrcName     — bare icon name without prefix or extension, e.g. "trash".
// customFill  — optional explicit CSS colour string (e.g. "#e0c060").
//               When empty the palette-derived colour is used.
QIcon svgIcon(const QString &qrcName, const QByteArray &customFill = {});

// Like svgIcon("folder") but rendered slightly lighter to distinguish folders
// from regular files at a glance.
QIcon svgFolderIcon(const QString &qrcName = QStringLiteral("folder"));

} // namespace linscp
