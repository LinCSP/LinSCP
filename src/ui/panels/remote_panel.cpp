#include "remote_panel.h"
#include "models/remote_fs_model.h"
#include "core/ssh/ssh_session.h"
#include "core/transfer/transfer_item.h"
#include "core/transfer/transfer_queue.h"
#include "ui/widgets/breadcrumb_bar.h"
#include "ui/widgets/file_list_view.h"
#include "ui/dialogs/copy_dialog.h"
#include "ui/dialogs/properties_dialog.h"
#include "ui/dialogs/remote_editor_dialog.h"
#include "utils/file_utils.h"
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QLabel>
#include <QTimer>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

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

    // Сортировка по клику на заголовок — in-memory, без SFTP-запроса
    connect(hdr, &QHeaderView::sectionClicked, this, [this, hdr](int logicalIndex) {
        const auto col = static_cast<models::RemoteFsModel::Column>(logicalIndex);
        Qt::SortOrder order = Qt::AscendingOrder;
        if (hdr->sortIndicatorSection() == logicalIndex
            && hdr->sortIndicatorOrder() == Qt::AscendingOrder)
            order = Qt::DescendingOrder;
        hdr->setSortIndicator(logicalIndex, order);
        m_model->setSortColumn(col, order);
    });
    hdr->setSortIndicatorShown(true);
    hdr->setSortIndicator(models::RemoteFsModel::ColName, Qt::AscendingOrder);
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
        auto *watcher = new QFutureWatcher<void>(this);
        connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
            watcher->deleteLater();
            refresh();
        });
        watcher->setFuture(QtConcurrent::run([sftp = m_sftp, paths]() {
            for (const QString &path : paths)
                sftp->removeRecursive(path);
        }));
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

    statusBar()->setText(tr("Deleting…"));

    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        refresh();
    });
    watcher->setFuture(QtConcurrent::run([sftp = m_sftp, paths]() {
        for (const QString &path : paths)
            sftp->removeRecursive(path);
    }));
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
    if (m_model->isDir(index)) {
        navigateTo(m_model->filePath(index));
    } else {
        // Открыть файл во встроенном редакторе
        dialogs::RemoteEditorDialog dlg(m_sftp, m_model->filePath(index), this);
        dlg.exec();
    }
}

// ── Контекстное меню (WinSCP-style) ──────────────────────────────────────────

void RemotePanel::populateContextMenu(QMenu *menu, const QModelIndex &index)
{
    const bool hasSelection = index.isValid();

    // ── Открыть / Редактировать ───────────────────────────────────────────────
    if (hasSelection) {
        if (m_model->isDir(index)) {
            menu->addAction(QIcon::fromTheme("folder-open"), tr("Open"), [this, index]() {
                navigateTo(m_model->filePath(index));
            });
        } else {
            menu->addAction(QIcon::fromTheme("document-edit"),
                            tr("Edit"),
                            [this, index]() {
                dialogs::RemoteEditorDialog dlg(m_sftp, m_model->filePath(index), this);
                dlg.exec();
            });
        }
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
    const int count = m_model->rowCount(m_model->indexForPath(currentPath()));

    if (!m_sshSession) {
        statusBar()->setText(tr("%1 items").arg(count));
        return;
    }

    // Запросить свободное место асинхронно — df блокирует сеть
    const QString snapshotPath = path.isEmpty() ? currentPath() : path;
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished,
            this, [this, watcher, count]() {
        watcher->deleteLater();
        const QString free = watcher->result();
        if (free.isEmpty())
            statusBar()->setText(tr("%1 items").arg(count));
        else
            statusBar()->setText(tr("%1 items  |  Free: %2").arg(count).arg(free));
    });
    watcher->setFuture(QtConcurrent::run(
        [this, snapshotPath]() { return queryFreeSpace(snapshotPath); }));
}

QString RemotePanel::queryFreeSpace(const QString &path) const
{
    if (!m_sshSession) return {};

    // df -P выводит строго-POSIX формат: Filesystem / 1024-blocks / Used / Available / ...
    const QString cmd = QStringLiteral("df -P '%1' 2>/dev/null | awk 'NR==2{print $4}'")
                            .arg(QString(path).replace('\'', "'\\''"));
    const QString out = m_sshSession->execCommand(cmd).trimmed();
    if (out.isEmpty()) return {};

    bool ok = false;
    const qint64 blocks = out.toLongLong(&ok);
    if (!ok || blocks < 0) return {};

    // df -P возвращает 1024-байтные блоки
    return utils::FileUtils::humanSize(blocks * 1024LL);
}

// ── Фильтры ───────────────────────────────────────────────────────────────────

void RemotePanel::setShowHiddenFiles(bool show)
{
    m_showHidden = show;
    m_model->setShowHidden(show);
}

} // namespace linscp::ui::panels
