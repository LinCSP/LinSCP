#include "remote_panel.h"
#include "models/remote_fs_model.h"
#include "core/transfer/transfer_item.h"
#include "core/transfer/transfer_queue.h"
#include "ui/widgets/breadcrumb_bar.h"
#include "ui/widgets/file_list_view.h"
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QLabel>

namespace linscp::ui::panels {

RemotePanel::RemotePanel(core::sftp::SftpClient *sftp,
                         core::transfer::TransferQueue *queue,
                         QWidget *parent)
    : FilePanel(Side::Remote, parent)
    , m_sftp(sftp)
    , m_queue(queue)
{
    m_model = new models::RemoteFsModel(sftp, this);

    listView()->setModel(m_model);
    breadcrumb()->setPath("/");

    connect(m_model, &models::RemoteFsModel::loadingStarted,
            this, &RemotePanel::onLoadingStarted);
    connect(m_model, &models::RemoteFsModel::loadingFinished,
            this, &RemotePanel::onLoadingFinished);

    navigateTo("/");
}

QString RemotePanel::currentPath() const
{
    return m_model->rootPath();
}

void RemotePanel::navigateTo(const QString &path)
{
    m_model->setRootPath(path);
    listView()->setRootIndex(m_model->indexForPath(path));
    breadcrumb()->setPath(path);
    emit pathChanged(path);
}

void RemotePanel::refresh()
{
    m_model->refresh();
}

QStringList RemotePanel::selectedPaths() const
{
    QStringList result;
    for (const QModelIndex &idx : listView()->selectionModel()->selectedRows())
        result << m_model->filePath(idx);
    return result;
}

void RemotePanel::downloadSelected(const QString &localDest)
{
    for (const QString &remotePath : selectedPaths()) {
        const QFileInfo fi(remotePath);
        core::transfer::TransferItem item;
        item.direction  = core::transfer::TransferDirection::Download;
        item.remotePath = remotePath;
        item.localPath  = localDest + '/' + fi.fileName();
        m_queue->enqueue(item);
    }
}

void RemotePanel::uploadFiles(const QStringList &localPaths)
{
    for (const QString &localPath : localPaths) {
        const QFileInfo fi(localPath);
        core::transfer::TransferItem item;
        item.direction  = core::transfer::TransferDirection::Upload;
        item.localPath  = localPath;
        item.remotePath = currentPath() + '/' + fi.fileName();
        item.totalBytes = fi.size();
        m_queue->enqueue(item);
    }
}

void RemotePanel::onItemActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;
    if (m_model->isDir(index))
        navigateTo(m_model->filePath(index));
    // TODO: открыть файл во встроенном редакторе
}

void RemotePanel::populateContextMenu(QMenu *menu, const QModelIndex &index)
{
    const bool hasSelection = index.isValid();

    if (hasSelection && m_model->isDir(index)) {
        menu->addAction(QIcon::fromTheme("folder-open"), tr("Open"), [this, index]() {
            navigateTo(m_model->filePath(index));
        });
    }

    if (hasSelection) {
        menu->addAction(QIcon::fromTheme("folder-download"), tr("Download"), [this]() {
            // TODO: выбор папки назначения
            downloadSelected(QDir::homePath());
        });
        menu->addSeparator();
        menu->addAction(tr("Rename…"), [this, index]() {
            bool ok = false;
            const QString newName = QInputDialog::getText(
                this, tr("Rename"), tr("New name:"), QLineEdit::Normal,
                m_model->fileInfo(index).name, &ok);
            if (ok && !newName.isEmpty()) {
                const QString oldPath = m_model->filePath(index);
                const QString newPath = oldPath.section('/', 0, -2) + '/' + newName;
                m_sftp->rename(oldPath, newPath);
                refresh();
            }
        });
        menu->addAction(QIcon::fromTheme("edit-delete"), tr("Delete"), [this, index]() {
            const QString path = m_model->filePath(index);
            if (QMessageBox::question(this, tr("Delete"),
                                      tr("Delete '%1'?").arg(path)) == QMessageBox::Yes) {
                if (m_model->isDir(index)) m_sftp->rmdir(path);
                else                       m_sftp->remove(path);
                refresh();
            }
        });
    }

    menu->addSeparator();
    menu->addAction(QIcon::fromTheme("folder-new"), tr("New folder…"), [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New Folder"),
                                                   tr("Folder name:"),
                                                   QLineEdit::Normal, {}, &ok);
        if (ok && !name.isEmpty()) {
            m_sftp->mkdir(currentPath() + '/' + name);
            refresh();
        }
    });
}

void RemotePanel::onLoadingStarted(const QString &path)
{
    statusBar()->setText(tr("Loading %1…").arg(path));
}

void RemotePanel::onLoadingFinished(const QString &path)
{
    Q_UNUSED(path);
    statusBar()->setText(tr("%1 items").arg(
        m_model->rowCount(m_model->indexForPath(currentPath()))));
}

} // namespace linscp::ui::panels
