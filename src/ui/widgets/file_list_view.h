#pragma once
#include <QTreeView>
#include <QSet>

namespace linscp::ui::widgets {

/// Кастомный QTreeView для панели файлов:
/// - Drag & Drop между панелями (local ↔ remote)
/// - Контекстное меню
/// - Выделение по пробелу / Ins (WinSCP-стиль checkbox tagging)
/// - Сортировка по клику на заголовок
/// - Настраиваемые колонки через контекстное меню заголовка
class FileListView : public QTreeView {
    Q_OBJECT
public:
    explicit FileListView(QWidget *parent = nullptr);

    /// Включить WinSCP-стиль тегирования файлов (Space/Ins + визуальный чекбокс)
    void setShowCheckboxes(bool show);
    bool showCheckboxes() const { return m_showCheckboxes; }

    /// Пути тегированных файлов (независимо от текущего выделения курсором)
    QStringList taggedPaths() const;
    void clearTags();
    bool isTagged(const QString &path) const { return m_taggedPaths.contains(path); }

    /// Включить remote-режим: при startDrag пакует пути в кастомный MIME,
    /// при drop принимает как local URLs, так и remote MIME.
    void setRemoteMode(bool remote);
    bool isRemoteMode() const { return m_remoteMode; }

    /// MIME-тип для перетаскивания удалённых файлов между панелями
    static constexpr const char *kRemoteMimeType = "application/x-linscp-remote-paths";

signals:
    void fileActivated(const QModelIndex &index);   ///< double-click или Enter
    void contextMenuRequested(const QModelIndex &index, const QPoint &globalPos);
    /// sourcePaths — пути источника, targetPath — целевая директория,
    /// fromRemote — true если источник — удалённая панель
    void dropToPath(const QStringList &sourcePaths,
                    const QString     &targetPath,
                    bool               fromRemote);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private slots:
    void onHeaderContextMenu(const QPoint &pos);

private:
    /// Переключить тег для строки по индексу (колонка 0)
    void toggleTag(const QModelIndex &idx);

    bool             m_showCheckboxes = false;
    bool             m_remoteMode     = false;
    QSet<QString>    m_taggedPaths;   ///< полные пути тегированных файлов
};

} // namespace linscp::ui::widgets
