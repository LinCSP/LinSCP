#pragma once
#include <QWidget>
#include <QModelIndex>

class QToolBar;
class QLabel;
class QSplitter;
class QMenu;

namespace linscp::ui::widgets {
class BreadcrumbBar;
class FileListView;
}

namespace linscp::ui::panels {

/// Базовый класс панели файлов (LocalPanel / RemotePanel).
/// Содержит: toolbar → breadcrumb → список файлов → статус-бар.
class FilePanel : public QWidget {
    Q_OBJECT
public:
    enum class Side { Local, Remote };

    explicit FilePanel(Side side, QWidget *parent = nullptr);
    ~FilePanel() override;

    Side side() const { return m_side; }

    virtual QString currentPath() const = 0;
    virtual void navigateTo(const QString &path) = 0;
    virtual void refresh() = 0;

    /// Список выбранных путей
    virtual QStringList selectedPaths() const = 0;

    /// Действия, вызываемые через горячие клавиши (F6/F7/F8)
    virtual void actionRename() {}
    virtual void actionMkdir()  {}
    virtual void actionDelete() {}

    /// Передать фокус списку файлов
    void setFocused();

signals:
    void pathChanged(const QString &path);
    void selectionChanged(const QStringList &paths);
    void uploadRequested(const QStringList &localPaths, const QString &remoteDest);
    void downloadRequested(const QStringList &remotePaths, const QString &localDest);

protected:
    widgets::BreadcrumbBar *breadcrumb() const { return m_breadcrumb; }
    widgets::FileListView  *listView()   const { return m_listView; }
    QToolBar               *toolBar()    const { return m_toolbar; }
    QLabel                 *statusBar()  const { return m_statusBar; }

    /// Вызывается при активации элемента (двойной клик / Enter)
    virtual void onItemActivated(const QModelIndex &index) = 0;

    /// Заполнить контекстное меню — переопределяется в подклассах
    virtual void populateContextMenu(QMenu *menu, const QModelIndex &index) = 0;

private slots:
    void onContextMenu(const QModelIndex &index, const QPoint &globalPos);
    void onBreadcrumbRequested(const QString &path);

private:
    void setupUi();
    void setupToolbar();

    Side                    m_side;
    QToolBar               *m_toolbar    = nullptr;
    widgets::BreadcrumbBar *m_breadcrumb = nullptr;
    widgets::FileListView  *m_listView   = nullptr;
    QLabel                 *m_statusBar  = nullptr;
};

} // namespace linscp::ui::panels
