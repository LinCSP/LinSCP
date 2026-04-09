#include "local_panel.h"
#include "models/local_fs_model.h"
#include "ui/widgets/breadcrumb_bar.h"
#include "ui/widgets/file_list_view.h"
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QMessageBox>

namespace linscp::ui::panels {

LocalPanel::LocalPanel(QWidget *parent)
    : FilePanel(Side::Local, parent)
{
    m_model = new models::LocalFsModel(this);
    m_model->setRootPath(QDir::homePath());

    listView()->setModel(m_model);
    listView()->setRootIndex(m_model->index(QDir::homePath()));

    // Скрыть лишние колонки QFileSystemModel
    for (int i = 1; i < m_model->columnCount(); ++i)
        listView()->hideColumn(i);

    breadcrumb()->setPath(QDir::homePath());

    connect(m_model, &models::LocalFsModel::rootPathChanged,
            this, [this](const QString &path) {
        breadcrumb()->setPath(path);
        emit pathChanged(path);
    });
}

QString LocalPanel::currentPath() const
{
    return m_model->rootPath();
}

void LocalPanel::navigateTo(const QString &path)
{
    const QString clean = QDir::cleanPath(path);
    if (!QDir(clean).exists()) return;

    m_model->setRootPath(clean);
    listView()->setRootIndex(m_model->index(clean));
    breadcrumb()->setPath(clean);
    emit pathChanged(clean);
}

void LocalPanel::refresh()
{
    // QFileSystemModel обновляется автоматически через inotify;
    // явного refresh достаточно через сброс root
    navigateTo(currentPath());
}

QStringList LocalPanel::selectedPaths() const
{
    QStringList result;
    for (const QModelIndex &idx : listView()->selectionModel()->selectedRows())
        result << m_model->filePath(idx);
    return result;
}

void LocalPanel::onItemActivated(const QModelIndex &index)
{
    const QString path = m_model->filePath(index);
    if (m_model->isDir(index))
        navigateTo(path);
    else
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void LocalPanel::populateContextMenu(QMenu *menu, const QModelIndex &index)
{
    const QString path = index.isValid() ? m_model->filePath(index) : currentPath();
    const bool isDir   = index.isValid() && m_model->isDir(index);

    if (isDir) {
        menu->addAction(QIcon::fromTheme("folder-open"), tr("Open"), [this, path]() {
            navigateTo(path);
        });
    }

    menu->addAction(QIcon::fromTheme("folder-new"), tr("New Folder"), [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New Folder"),
                                                   tr("Folder name:"),
                                                   QLineEdit::Normal, {}, &ok);
        if (ok && !name.isEmpty())
            QDir(currentPath()).mkdir(name);
    });

    if (index.isValid()) {
        menu->addSeparator();
        menu->addAction(QIcon::fromTheme("edit-delete"), tr("Delete"), [this, path]() {
            if (QMessageBox::question(this, tr("Delete"),
                                      tr("Delete '%1'?").arg(path)) == QMessageBox::Yes)
                QFile::remove(path);
        });
    }
}

} // namespace linscp::ui::panels
