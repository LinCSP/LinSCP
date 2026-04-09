#pragma once
#include <QTreeView>

namespace linscp::ui::widgets {

/// Кастомный QTreeView для панели файлов:
/// - Drag & Drop между панелями
/// - Контекстное меню
/// - Выделение по пробелу (WinSCP-стиль)
/// - Сортировка по клику на заголовок
class FileListView : public QTreeView {
    Q_OBJECT
public:
    explicit FileListView(QWidget *parent = nullptr);

    void setShowCheckboxes(bool show);

signals:
    void fileActivated(const QModelIndex &index);   ///< double-click или Enter
    void contextMenuRequested(const QModelIndex &index, const QPoint &globalPos);
    void dropToPath(const QStringList &sourcePaths, const QString &targetPath);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    bool m_showCheckboxes = false;
};

} // namespace linscp::ui::widgets
