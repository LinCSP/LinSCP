#include "file_list_view.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDrag>
#include <QHeaderView>
#include <QApplication>

namespace linscp::ui::widgets {

FileListView::FileListView(QWidget *parent)
    : QTreeView(parent)
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::CopyAction);
    setRootIsDecorated(false);
    setUniformRowHeights(true);
    setSortingEnabled(true);
    setAlternatingRowColors(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    header()->setSectionsMovable(true);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
}

void FileListView::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QModelIndex idx = indexAt(event->pos());
    if (idx.isValid())
        emit fileActivated(idx);
    QTreeView::mouseDoubleClickEvent(event);
}

void FileListView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const QModelIndex idx = currentIndex();
        if (idx.isValid()) emit fileActivated(idx);
        return;
    }
    QTreeView::keyPressEvent(event);
}

void FileListView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex idx = indexAt(event->pos());
    emit contextMenuRequested(idx, event->globalPos());
}

void FileListView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText())
        event->acceptProposedAction();
}

void FileListView::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void FileListView::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) return;

    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls())
        paths << url.toLocalFile();

    const QModelIndex target = indexAt(event->position().toPoint());
    const QString targetPath = target.isValid()
                                   ? target.data(Qt::UserRole + 1).toString()
                                   : rootIndex().data(Qt::UserRole + 1).toString();

    if (!paths.isEmpty())
        emit dropToPath(paths, targetPath);

    event->acceptProposedAction();
}

void FileListView::startDrag(Qt::DropActions supportedActions)
{
    const QModelIndexList selected = selectedIndexes();
    if (selected.isEmpty()) return;

    QList<QUrl> urls;
    QStringList names;
    for (const QModelIndex &idx : selected) {
        if (idx.column() != 0) continue;
        const QString path = idx.data(Qt::UserRole + 1).toString();
        if (!path.isEmpty()) {
            urls << QUrl::fromLocalFile(path);
            names << idx.data(Qt::DisplayRole).toString();
        }
    }

    if (urls.isEmpty()) return;

    auto *drag = new QDrag(this);
    auto *mime = new QMimeData;
    mime->setUrls(urls);
    mime->setText(names.join('\n'));
    drag->setMimeData(mime);
    drag->exec(supportedActions, Qt::CopyAction);
}

} // namespace linscp::ui::widgets
