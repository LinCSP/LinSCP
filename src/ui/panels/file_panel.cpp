#include "file_panel.h"
#include "ui/widgets/breadcrumb_bar.h"
#include "ui/widgets/file_list_view.h"
#include "ui/utils/svg_icon.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QTimer>

namespace linscp::ui::panels {

FilePanel::FilePanel(Side side, QWidget *parent)
    : QWidget(parent), m_side(side)
{
    setupUi();
}

FilePanel::~FilePanel() = default;

void FilePanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Toolbar
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(16, 16));
    setupToolbar();

    // Breadcrumb
    m_breadcrumb = new widgets::BreadcrumbBar(this);
    connect(m_breadcrumb, &widgets::BreadcrumbBar::pathRequested,
            this, &FilePanel::onBreadcrumbRequested);

    // File list — remote mode устанавливается в конструкторах подклассов
    m_listView = new widgets::FileListView(this);
    connect(m_listView, &widgets::FileListView::fileActivated,
            this, &FilePanel::onItemActivated);
    connect(m_listView, &widgets::FileListView::contextMenuRequested,
            this, &FilePanel::onContextMenu);
    connect(m_listView, &widgets::FileListView::dropToPath,
            this, &FilePanel::onDropToPath);

    // Status bar
    m_statusBar = new QLabel(this);
    m_statusBar->setContentsMargins(4, 2, 4, 2);
    m_statusBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    layout->addWidget(m_toolbar);
    layout->addWidget(m_breadcrumb);
    layout->addWidget(m_listView, 1);
    layout->addWidget(m_statusBar);
}

void FilePanel::setupToolbar()
{
    m_toolbar->addAction(svgIcon(QStringLiteral("arrow-up")),
                         tr("Up"),
                         [this]() { navigateTo(currentPath() + "/.."); });

    m_toolbar->addAction(svgIcon(QStringLiteral("rotate")),
                         tr("Refresh"),
                         [this]() { refresh(); });

    m_toolbar->addSeparator();

    m_toolbar->addAction(svgIcon(QStringLiteral("folder-plus")),
                         tr("New folder"),
                         [this]() { actionMkdir(); });

    m_toolbar->addAction(svgIcon(QStringLiteral("trash")),
                         tr("Delete"),
                         [this]() { actionDelete(); });
}

void FilePanel::onContextMenu(const QModelIndex &index, const QPoint &globalPos)
{
    QMenu menu(this);
    populateContextMenu(&menu, index);
    if (!menu.isEmpty())
        menu.exec(globalPos);
}

void FilePanel::setFocused()
{
    m_listView->setFocus();
}

void FilePanel::onBreadcrumbRequested(const QString &path)
{
    navigateTo(path);
}

void FilePanel::onDropToPath(const QStringList &sourcePaths,
                             const QString     &targetPath,
                             bool               fromRemote)
{
    // Откладываем на следующий тик — нельзя открывать диалог внутри dropEvent
    QTimer::singleShot(0, this, [this, sourcePaths, targetPath, fromRemote]() {
        if (m_side == Side::Local && fromRemote) {
            // Remote → Local: скачать
            const QString dest = targetPath.isEmpty() ? currentPath() : targetPath;
            emit downloadRequested(sourcePaths, dest);
        } else if (m_side == Side::Remote && !fromRemote) {
            // Local → Remote: загрузить
            emit uploadRequested(sourcePaths, currentPath());
        }
        // Local→Local или Remote→Remote обрабатывают подклассы
    });
}

} // namespace linscp::ui::panels
