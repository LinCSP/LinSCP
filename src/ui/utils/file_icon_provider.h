#pragma once

#include "svg_icon.h"
#include <QAbstractFileIconProvider>
#include <QIcon>
#include <QMimeType>

namespace linscp {

// Returns an FA-based icon for a MIME type — consistent across all themes.
QIcon iconForMime(const QMimeType &mime);

// QAbstractFileIconProvider implementation for QFileSystemModel (local panel).
class FileIconProvider final : public QAbstractFileIconProvider
{
public:
    QIcon icon(const QFileInfo &info) const override;
    QIcon icon(IconType type)         const override;
};

} // namespace linscp
