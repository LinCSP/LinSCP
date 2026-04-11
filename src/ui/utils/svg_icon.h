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
QIcon svgIcon(const QString &qrcName);

} // namespace linscp
