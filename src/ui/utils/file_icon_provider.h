#pragma once

#include "svg_icon.h"
#include <QAbstractFileIconProvider>
#include <QIcon>
#include <QMimeType>
#include <QHash>
#include <QString>

namespace linscp {

// Free function for RemoteFsModel — safe to call from GUI thread only.
QIcon iconForMime(const QMimeType &mime);

// QAbstractFileIconProvider implementation for QFileSystemModel.
// icon(QFileInfo) returns QIcon() — QFileSystemModel calls it from a background thread
// and Qt docs forbid using QIcon off the GUI thread.
// Use iconForFile() / iconForMimeType() from the GUI thread (e.g. in model::data()).
class FileIconProvider final : public QAbstractFileIconProvider
{
public:
    FileIconProvider();

    QIcon icon(const QFileInfo &info) const override;
    QIcon icon(IconType type)         const override;

    // GUI-thread-only helpers (called from model::data(), not from FileInfoGatherer).
    QIcon iconForFile(const QFileInfo &info) const;
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
