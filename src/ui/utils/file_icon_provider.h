#pragma once

#include "svg_icon.h"
#include <QAbstractFileIconProvider>
#include <QIcon>
#include <QMimeType>
#include <QHash>
#include <QString>

namespace linscp {

// QAbstractFileIconProvider implementation for QFileSystemModel.
// All QIcon objects are pre-built in the constructor (GUI thread).
// icon() only returns cached values — safe to call from any thread.
class FileIconProvider final : public QAbstractFileIconProvider
{
public:
    FileIconProvider();

    QIcon icon(const QFileInfo &info) const override;
    QIcon icon(IconType type)         const override;

    // Used by iconForMime() free function (GUI thread only).
    const QIcon &iconForMimeType(const QString &mimeType) const;

private:
    // Pre-built icons (created once in GUI thread)
    QIcon m_folder;
    QIcon m_file;
    QIcon m_fileZip;
    QIcon m_filePdf;
    QIcon m_fileImage;
    QIcon m_fileAudio;
    QIcon m_fileVideo;
    QIcon m_fileCode;
    QIcon m_fileText;
    QIcon m_link;
};

} // namespace linscp
