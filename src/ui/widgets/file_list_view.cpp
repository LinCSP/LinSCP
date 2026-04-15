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
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QStyle>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QPixmap>
#include <QPainter>

namespace linscp::ui::widgets {

// ─── CheckboxDelegate ─────────────────────────────────────────────────────────
/// Делегат колонки 0: рисует небольшой чекбокс слева от иконки.
/// Визуально тегированные файлы выделяются отдельным маркером независимо
/// от текущего выделения курсором (аналог WinSCP Insert/Space).
class CheckboxDelegate : public QStyledItemDelegate {
    FileListView *m_view;
    static constexpr int kBoxW = 16;
    static constexpr int kPad  = 2;

public:
    explicit CheckboxDelegate(FileListView *view)
        : QStyledItemDelegate(view), m_view(view) {}

    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &idx) const override
    {
        if (idx.column() != 0) {
            QStyledItemDelegate::paint(p, opt, idx);
            return;
        }

        const QString path = idx.data(Qt::UserRole + 1).toString();
        const bool tagged  = m_view->isTagged(path);

        // Рисуем стандартный элемент, сдвинув rect вправо под чекбокс
        QStyleOptionViewItem shifted = opt;
        shifted.rect.adjust(kBoxW + kPad * 2, 0, 0, 0);
        QStyledItemDelegate::paint(p, shifted, idx);

        // Рисуем чекбокс-индикатор в левой части строки
        QStyleOptionButton cbOpt;
        const int cy = opt.rect.top() + (opt.rect.height() - kBoxW) / 2;
        cbOpt.rect  = QRect(opt.rect.left() + kPad, cy, kBoxW, kBoxW);
        cbOpt.state = QStyle::State_Enabled;
        if (tagged)
            cbOpt.state |= QStyle::State_On;
        else
            cbOpt.state |= QStyle::State_Off;

        // Фон для тегированной строки — мягкий акцентный цвет
        if (tagged) {
            const QColor accent = opt.palette.color(QPalette::Highlight).lighter(175);
            p->fillRect(opt.rect, accent);
        }

        opt.widget
            ? opt.widget->style()->drawControl(QStyle::CE_CheckBox, &cbOpt, p, opt.widget)
            : QApplication::style()->drawControl(QStyle::CE_CheckBox, &cbOpt, p);
    }

    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &idx) const override
    {
        QSize sz = QStyledItemDelegate::sizeHint(opt, idx);
        if (idx.column() == 0)
            sz.rwidth() += kBoxW + kPad * 2;
        return sz;
    }
};

// ─── FileListView ─────────────────────────────────────────────────────────────

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
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QHeaderView::customContextMenuRequested,
            this, &FileListView::onHeaderContextMenu);
    // setSectionResizeMode(0, ...) нельзя вызывать без модели (нет секций → crash на Windows).
    // Вызывается в LocalPanel/RemotePanel после setModel().
}

void FileListView::setShowCheckboxes(bool show)
{
    if (m_showCheckboxes == show) return;
    m_showCheckboxes = show;
    if (show) {
        setItemDelegateForColumn(0, new CheckboxDelegate(this));
    } else {
        setItemDelegateForColumn(0, nullptr);
        m_taggedPaths.clear();
    }
    viewport()->update();
}

QStringList FileListView::taggedPaths() const
{
    return QStringList(m_taggedPaths.cbegin(), m_taggedPaths.cend());
}

void FileListView::clearTags()
{
    if (m_taggedPaths.isEmpty()) return;
    m_taggedPaths.clear();
    viewport()->update();
}

void FileListView::toggleTag(const QModelIndex &idx)
{
    if (!idx.isValid()) return;
    const QModelIndex col0 = idx.sibling(idx.row(), 0);
    const QString path = col0.data(Qt::UserRole + 1).toString();
    if (path.isEmpty()) return;
    if (m_taggedPaths.contains(path))
        m_taggedPaths.remove(path);
    else
        m_taggedPaths.insert(path);
    // Перерисовать строку
    const QRect r = visualRect(col0);
    viewport()->update(r);
}

void FileListView::setRemoteMode(bool remote)
{
    m_remoteMode = remote;
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void FileListView::mousePressEvent(QMouseEvent *event)
{
    if (m_showCheckboxes && event->button() == Qt::LeftButton) {
        const QModelIndex idx = indexAt(event->pos());
        if (idx.isValid() && idx.column() == 0) {
            // Левее иконки ~18px — область чекбокса
            const QRect itemRect = visualRect(idx);
            if (event->pos().x() < itemRect.left() + 20) {
                toggleTag(idx);
                return;
            }
        }
    }
    QTreeView::mousePressEvent(event);
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
    // WinSCP-стиль: Space / Insert — тегировать файл и перейти к следующему
    if (m_showCheckboxes &&
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Insert))
    {
        const QModelIndex idx = currentIndex();
        if (idx.isValid()) {
            toggleTag(idx);
            // Сдвигаемся на следующую строку (как в WinSCP)
            const QModelIndex next = idx.sibling(idx.row() + 1, idx.column());
            if (next.isValid()) setCurrentIndex(next);
        }
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

// ── Настраиваемые колонки ─────────────────────────────────────────────────────

void FileListView::onHeaderContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    menu.setTitle(tr("Columns"));

    const int count = header()->count();
    for (int i = 1; i < count; ++i) {   // колонка 0 (Name) всегда видима
        const QString title = model() ? model()->headerData(i, Qt::Horizontal).toString()
                                      : QString::number(i);
        QAction *act = menu.addAction(title);
        act->setCheckable(true);
        act->setChecked(!header()->isSectionHidden(i));
        act->setData(i);
    }

    QAction *chosen = menu.exec(header()->mapToGlobal(pos));
    if (!chosen) return;

    const int section = chosen->data().toInt();
    const bool hide   = header()->isSectionHidden(section);
    header()->setSectionHidden(section, !hide);
}

} // namespace linscp::ui::widgets
