#include "remote_panel.h"
#include "models/remote_fs_model.h"
#include "core/transfer/transfer_item.h"
#include "core/transfer/transfer_queue.h"
#include "ui/widgets/breadcrumb_bar.h"
#include "ui/widgets/file_list_view.h"
#include "ui/dialogs/copy_dialog.h"
#include "ui/dialogs/properties_dialog.h"
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QLabel>
#include <QTimer>

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
    listView()->setRemoteMode(true);    // remote DnD MIME

    // Колонки: 0=Name, 1=Size, 2=Modified, 3=Permissions, 4=Owner
    auto *hdr = listView()->header();
    hdr->setSectionResizeMode(0, QHeaderView::Stretch);
    hdr->setSectionResizeMode(1, QHeaderView::Fixed);
    hdr->setSectionResizeMode(2, QHeaderView::Fixed);
    hdr->setSectionResizeMode(3, QHeaderView::Fixed);
    hdr->resizeSection(1, 75);
    hdr->resizeSection(2, 130);
    hdr->resizeSection(3, 100);
    listView()->hideColumn(models::RemoteFsModel::ColOwner);

    breadcrumb()->setPath("/");

    connect(m_model, &models::RemoteFsModel::loadingStarted,
            this, &RemotePanel::onLoadingStarted);
    connect(m_model, &models::RemoteFsModel::loadingFinished,
            this, &RemotePanel::onLoadingFinished);
}

// ── Навигация ─────────────────────────────────────────────────────────────────

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

// ── Передача файлов ───────────────────────────────────────────────────────────

void RemotePanel::downloadSelected(const QString &localDest, bool isMove)
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;

    QString dest = localDest;
    if (dest.isEmpty()) {
        dialogs::CopyDialog dlg(dialogs::CopyDialog::Direction::Download,
                                isMove, paths,
                                QDir::homePath(), this);
        if (dlg.exec() != QDialog::Accepted) return;
        dest = dlg.targetDirectory();
    }

    for (const QString &remotePath : paths) {
        const QFileInfo fi(remotePath);
        core::transfer::TransferItem item;
        item.direction  = core::transfer::TransferDirection::Download;
        item.remotePath = remotePath;
        item.localPath  = dest + '/' + fi.fileName();
        m_queue->enqueue(item);
    }

    if (isMove) {
        // Удалить remote файлы после постановки в очередь
        // (TransferManager завершит перенос, удаление сразу — acceptable для Move)
        for (const QString &path : paths) {
            const QModelIndex idx = m_model->indexForPath(path);
            if (idx.isValid() && m_model->isDir(idx))
                m_sftp->rmdir(path);
            else
                m_sftp->remove(path);
        }
        refresh();
    }
}

void RemotePanel::uploadFiles(const QStringList &localPaths, bool isMove)
{
    if (localPaths.isEmpty()) return;

    dialogs::CopyDialog dlg(dialogs::CopyDialog::Direction::Upload,
                            isMove, localPaths,
                            currentPath(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString destPath = dlg.targetDirectory();

    for (const QString &localPath : localPaths) {
        const QFileInfo fi(localPath);
        core::transfer::TransferItem item;
        item.direction  = core::transfer::TransferDirection::Upload;
        item.localPath  = localPath;
        item.remotePath = destPath + '/' + fi.fileName();
        item.totalBytes = fi.size();
        m_queue->enqueue(item);
    }
    // isMove: локальные файлы удаляет вызывающий (LocalPanel::actionMove)
}

// ── Действия (F-клавиши) ──────────────────────────────────────────────────────

void RemotePanel::actionCopy()
{
    downloadSelected({}, /*isMove=*/false);
}

void RemotePanel::actionMove()
{
    downloadSelected({}, /*isMove=*/true);
}

void RemotePanel::actionMkdir()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Folder"),
                                               tr("Folder name:"),
                                               QLineEdit::Normal, {}, &ok);
    if (ok && !name.isEmpty()) {
        m_sftp->mkdir(currentPath() + '/' + name);
        refresh();
    }
}

void RemotePanel::actionDelete()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;
    if (QMessageBox::question(this, tr("Delete"),
                              tr("Delete %n item(s)?", nullptr, paths.size()))
            != QMessageBox::Yes)
        return;
    for (const QString &path : paths) {
        const QModelIndex idx = m_model->indexForPath(path);
        if (idx.isValid() && m_model->isDir(idx))
            m_sftp->rmdir(path);
        else
            m_sftp->remove(path);
    }
    refresh();
}

void RemotePanel::actionRename()
{
    const QModelIndex idx = listView()->currentIndex();
    if (!idx.isValid()) return;
    const QString oldPath = m_model->filePath(idx);
    bool ok = false;
    const QString newName = QInputDialog::getText(
        this, tr("Rename"), tr("New name:"),
        QLineEdit::Normal, m_model->fileInfo(idx).name, &ok);
    if (ok && !newName.isEmpty()) {
        const QString newPath = oldPath.section('/', 0, -2) + '/' + newName;
        m_sftp->rename(oldPath, newPath);
        refresh();
    }
}

// ── Активация ─────────────────────────────────────────────────────────────────

void RemotePanel::onItemActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;
    if (m_model->isDir(index))
        navigateTo(m_model->filePath(index));
    // TODO: открыть файл во встроенном редакторе
}

// ── Контекстное меню (WinSCP-style) ──────────────────────────────────────────

void RemotePanel::populateContextMenu(QMenu *menu, const QModelIndex &index)
{
    const bool hasSelection = index.isValid();

    // ── Открыть ───────────────────────────────────────────────────────────────
    if (hasSelection && m_model->isDir(index)) {
        menu->addAction(QIcon::fromTheme("folder-open"), tr("Open"), [this, index]() {
            navigateTo(m_model->filePath(index));
        });
        menu->addSeparator();
    }

    // ── Скачать / Переместить ─────────────────────────────────────────────────
    if (hasSelection) {
        menu->addAction(QIcon::fromTheme("folder-download"),
                        tr("Download…\tF5"),
                        [this]() { actionCopy(); });

        menu->addAction(QIcon::fromTheme("folder-download"),
                        tr("Download and Delete…\tF6"),
                        [this]() { actionMove(); });

        // Скачать в фоне (добавить в очередь без диалога)
        menu->addAction(QIcon::fromTheme("folder-download"),
                        tr("Download to Queue"),
                        [this]() {
            const QStringList paths = selectedPaths();
            for (const QString &remotePath : paths) {
                core::transfer::TransferItem item;
                item.direction  = core::transfer::TransferDirection::Download;
                item.remotePath = remotePath;
                item.localPath  = QDir::homePath() + '/' + QFileInfo(remotePath).fileName();
                m_queue->enqueue(item);
            }
        });

        menu->addSeparator();
    }

    // ── Правка ────────────────────────────────────────────────────────────────
    if (hasSelection) {
        menu->addAction(QIcon::fromTheme("edit-rename"),
                        tr("Rename…\tF2"),
                        [this]() { actionRename(); });

        menu->addAction(QIcon::fromTheme("edit-delete"),
                        tr("Delete\tDel"),
                        [this]() { actionDelete(); });

        menu->addSeparator();
    }

    // ── Создать папку ─────────────────────────────────────────────────────────
    menu->addAction(QIcon::fromTheme("folder-new"),
                    tr("New Folder…\tF7"),
                    [this]() { actionMkdir(); });

    // ── Свойства ──────────────────────────────────────────────────────────────
    if (hasSelection) {
        menu->addSeparator();
        menu->addAction(QIcon::fromTheme("document-properties"),
                        tr("Properties…"),
                        [this, index]() {
            const core::sftp::SftpFileInfo info = m_model->fileInfo(index);
            dialogs::PropertiesDialog dlg(info, m_sftp, this);
            dlg.exec();
            refresh();
        });
    }
}

// ── Drag & Drop ───────────────────────────────────────────────────────────────

void RemotePanel::onDropToPath(const QStringList &sourcePaths,
                               const QString     &targetPath,
                               bool               fromRemote)
{
    if (fromRemote) return;  // Remote → Remote: не поддерживается

    // Откладываем — нельзя открывать диалог внутри dropEvent
    QTimer::singleShot(0, this, [this, sourcePaths, targetPath]() {
        // Local → Remote: показываем CopyDialog и ставим в очередь
        const QString dest = targetPath.isEmpty() ? currentPath() : targetPath;
        dialogs::CopyDialog dlg(dialogs::CopyDialog::Direction::Upload,
                                /*isMove=*/false, sourcePaths,
                                dest, this);
        if (dlg.exec() != QDialog::Accepted) return;
        const QString finalDest = dlg.targetDirectory();

        for (const QString &localPath : sourcePaths) {
            const QFileInfo fi(localPath);
            core::transfer::TransferItem item;
            item.direction  = core::transfer::TransferDirection::Upload;
            item.localPath  = localPath;
            item.remotePath = finalDest + '/' + fi.fileName();
            item.totalBytes = fi.size();
            m_queue->enqueue(item);
        }
    });
}

// ── Статусная строка ──────────────────────────────────────────────────────────

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

// ── Фильтры ───────────────────────────────────────────────────────────────────

void RemotePanel::setShowHiddenFiles(bool show)
{
    m_showHidden = show;
    m_model->setShowHidden(show);
}

} // namespace linscp::ui::panels
