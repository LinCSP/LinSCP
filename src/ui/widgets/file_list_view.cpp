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
#include <QPixmap>
#include <QPainter>

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
    // setSectionResizeMode(0, ...) нельзя вызывать без модели (нет секций → crash на Windows).
    // Вызывается в LocalPanel/RemotePanel после setModel().
}

void FileListView::setRemoteMode(bool remote)
{
    m_remoteMode = remote;
}

// ── Активация ─────────────────────────────────────────────────────────────────

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

// ── Drag & Drop ───────────────────────────────────────────────────────────────

void FileListView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() ||
        event->mimeData()->hasFormat(kRemoteMimeType))
        event->acceptProposedAction();
    else
        event->ignore();
}

void FileListView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls() ||
        event->mimeData()->hasFormat(kRemoteMimeType))
        event->acceptProposedAction();
    else
        event->ignore();
}

void FileListView::dropEvent(QDropEvent *event)
{
    const bool fromRemote = event->mimeData()->hasFormat(kRemoteMimeType);

    QStringList paths;
    if (fromRemote) {
        const QString raw = QString::fromUtf8(
            event->mimeData()->data(kRemoteMimeType));
        paths = raw.split('\n', Qt::SkipEmptyParts);
    } else if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            const QString p = url.toLocalFile();
            if (!p.isEmpty()) paths << p;
        }
    }

    if (paths.isEmpty()) { event->ignore(); return; }

    // Определяем целевую директорию:
    // если дроп на файл/папку — берём путь этого элемента,
    // иначе — текущий корень панели (Qt::UserRole+1 содержит полный путь).
    const QModelIndex target = indexAt(event->position().toPoint());
    const QString targetPath = target.isValid()
        ? target.data(Qt::UserRole + 1).toString()
        : rootIndex().data(Qt::UserRole + 1).toString();

    emit dropToPath(paths, targetPath, fromRemote);
    event->acceptProposedAction();
}

void FileListView::startDrag(Qt::DropActions supportedActions)
{
    const QModelIndexList selected = selectedIndexes();
    if (selected.isEmpty()) return;

    QList<QUrl> urls;
    QStringList remotePaths;
    QStringList names;

    for (const QModelIndex &idx : selected) {
        if (idx.column() != 0) continue;
        const QString path = idx.data(Qt::UserRole + 1).toString();
        if (path.isEmpty()) continue;
        names << idx.data(Qt::DisplayRole).toString();
        if (m_remoteMode) {
            remotePaths << path;
        } else {
            urls << QUrl::fromLocalFile(path);
        }
    }

    if (names.isEmpty()) return;

    auto *drag = new QDrag(this);
    auto *mime = new QMimeData;

    if (m_remoteMode) {
        mime->setData(kRemoteMimeType, remotePaths.join('\n').toUtf8());
        // Также добавляем текст для отладки
        mime->setText(remotePaths.join('\n'));
    } else {
        mime->setUrls(urls);
        mime->setText(names.join('\n'));
    }

    drag->setMimeData(mime);

    // Иконка-превью: имя первого файла + счётчик если несколько
    if (names.size() > 1) {
        const QString label = tr("%1 files").arg(names.size());
        QPixmap px(200, 24);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setPen(palette().color(QPalette::Text));
        p.drawText(px.rect(), Qt::AlignVCenter | Qt::AlignLeft, label);
        drag->setPixmap(px);
    }

    drag->exec(supportedActions, Qt::CopyAction);
}

} // namespace linscp::ui::widgets
