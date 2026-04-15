#include "local_panel.h"
#include "core/app_settings.h"
#include "models/local_fs_model.h"
#include "ui/widgets/breadcrumb_bar.h"
#include "ui/widgets/file_list_view.h"
#include "ui/utils/svg_icon.h"
#include <QHeaderView>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace {

/// Открывает локальный терминал в директории dir.
/// Использует настройку AppSettings::terminalMode() — тот же эмулятор что и для SSH.
static void openLocalTerminal(const QString &dir)
{
    using Mode = linscp::core::AppSettings::TerminalMode;
    const Mode mode = linscp::core::AppSettings::terminalMode();

    auto tryLaunch = [&](const QString &bin) -> bool {
        const QString path = QStandardPaths::findExecutable(bin);
        if (path.isEmpty()) return false;
        QProcess::startDetached(path, {}, dir);
        return true;
    };

    if (mode == Mode::Custom) {
        const QString custom = linscp::core::AppSettings::terminalCustomPath();
        if (!custom.isEmpty()) QProcess::startDetached(custom, {}, dir);
        return;
    }

    if (mode != Mode::AutoDetect) {
        const QString bin = linscp::core::AppSettings::terminalBinaryForMode(mode);
        if (!bin.isEmpty()) tryLaunch(bin);
        return;
    }

    // AutoDetect — тот же приоритет что в TerminalWidget
    for (const char *bin : { "kitty", "konsole", "gnome-terminal",
                              "xfce4-terminal", "alacritty", "xterm",
                              "lxterminal", "mate-terminal", "tilix" }) {
        if (tryLaunch(bin)) return;
    }
}

/// Рекурсивно копирует файл или директорию src в destDir/name(src).
/// Возвращает false при первой ошибке.
bool copyRecursively(const QString &src, const QString &destDir)
{
    const QFileInfo fi(src);
    const QString dst = destDir + '/' + fi.fileName();
    if (fi.isDir()) {
        if (!QDir(destDir).mkdir(fi.fileName()))
            return false;
        for (const QString &child : QDir(src).entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            if (!copyRecursively(src + '/' + child, dst))
                return false;
        }
        return true;
    }
    return QFile::copy(src, dst);
}

} // anonymous namespace

namespace linscp::ui::panels {

LocalPanel::LocalPanel(QWidget *parent)
    : FilePanel(Side::Local, parent)
{
    m_model = new models::LocalFsModel(this);
    m_model->setRootPath(QDir::homePath());

    listView()->setModel(m_model);
    listView()->setRootIndex(m_model->index(QDir::homePath()));
    listView()->setRemoteMode(false);

    // Колонки QFileSystemModel: 0=Name, 1=Size, 2=Type, 3=Date Modified
    listView()->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    listView()->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    listView()->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    listView()->header()->setSectionResizeMode(3, QHeaderView::Fixed);
    listView()->header()->resizeSection(1, 75);
    listView()->header()->resizeSection(2, 95);
    listView()->header()->resizeSection(3, 130);

    breadcrumb()->setPath(QDir::homePath());

    connect(m_model, &models::LocalFsModel::rootPathChanged,
            this, [this](const QString &path) {
        breadcrumb()->setPath(path);
        emit pathChanged(path);
    });

    // Сигналим при смене сортировки — чтобы MainWindow мог сохранить в QSettings
    connect(listView()->header(), &QHeaderView::sortIndicatorChanged,
            this, [this](int col, Qt::SortOrder ord) {
        emit sortStateChanged(col, static_cast<int>(ord));
    });
}

// ── Навигация ─────────────────────────────────────────────────────────────────

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
    navigateTo(currentPath());
}

QStringList LocalPanel::selectedPaths() const
{
    QStringList result;
    for (const QModelIndex &idx : listView()->selectionModel()->selectedRows())
        result << m_model->filePath(idx);
    return result;
}

// ── Действия (F-клавиши) ──────────────────────────────────────────────────────

void LocalPanel::actionCopy()
{
    // F5 — запросить upload выбранных файлов на remote
    const QStringList paths = selectedPaths();
    if (!paths.isEmpty())
        emit uploadRequested(paths, QString{});
}

void LocalPanel::actionMove()
{
    // F6 — Upload + Delete local после подтверждения
    const QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;

    if (QMessageBox::question(this, tr("Move to Remote"),
                              tr("Upload %n item(s) and delete local copies?",
                                 nullptr, paths.size()))
            != QMessageBox::Yes)
        return;

    emit uploadRequested(paths, QString{});  // ConnectionTab должен удалить после загрузки
    for (const QString &p : paths)
        QFile::remove(p);
}

void LocalPanel::actionMkdir()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Folder"),
                                               tr("Folder name:"),
                                               QLineEdit::Normal, {}, &ok);
    if (ok && !name.isEmpty())
        QDir(currentPath()).mkdir(name);
}

void LocalPanel::actionDelete()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty()) return;
    if (QMessageBox::question(this, tr("Delete"),
                              tr("Delete %n item(s)?", nullptr, paths.size()))
            != QMessageBox::Yes)
        return;

    for (const QString &p : paths) {
        if (QFileInfo(p).isDir())
            QDir(p).removeRecursively();
        else
            QFile::remove(p);
    }
    refresh();
}

void LocalPanel::actionRename()
{
    const QModelIndex idx = listView()->currentIndex();
    if (!idx.isValid()) return;
    const QString oldPath = m_model->filePath(idx);
    bool ok = false;
    const QString newName = QInputDialog::getText(
        this, tr("Rename"), tr("New name:"),
        QLineEdit::Normal, QFileInfo(oldPath).fileName(), &ok);
    if (ok && !newName.isEmpty())
        QFile::rename(oldPath, QFileInfo(oldPath).dir().absoluteFilePath(newName));
}

// ── Активация ─────────────────────────────────────────────────────────────────

void LocalPanel::onItemActivated(const QModelIndex &index)
{
    const QString path = m_model->filePath(index);
    if (m_model->isDir(index))
        navigateTo(path);
    else
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

// ── Контекстное меню ──────────────────────────────────────────────────────────

void LocalPanel::populateContextMenu(QMenu *menu, const QModelIndex &index)
{
    const bool hasSelection = index.isValid();
    const QString path = hasSelection ? m_model->filePath(index) : currentPath();
    const bool isDir   = hasSelection && m_model->isDir(index);

    // Открыть
    if (isDir) {
        menu->addAction(svgIcon(QStringLiteral("folder-open")), tr("Open"), [this, path]() {
            navigateTo(path);
        });
        menu->addSeparator();
    }

    // ── Передача на remote ───────────────────────────────────────────────────
    if (hasSelection) {
        auto *copyAct = menu->addAction(
            svgIcon(QStringLiteral("upload")), tr("Upload…\tF5"),
            [this]() { actionCopy(); });
        copyAct->setEnabled(true);

        auto *moveAct = menu->addAction(
            svgIcon(QStringLiteral("upload")), tr("Move to Remote…\tF6"),
            [this]() { actionMove(); });
        moveAct->setEnabled(true);

        menu->addSeparator();
    }

    // ── Правка ───────────────────────────────────────────────────────────────
    if (hasSelection) {
        menu->addAction(svgIcon(QStringLiteral("pen")), tr("Rename…\tF2"),
                        [this]() { actionRename(); });

        menu->addAction(svgIcon(QStringLiteral("trash")), tr("Delete\tDel"),
                        [this]() { actionDelete(); });

        menu->addSeparator();
    }

    // ── Новая папка ───────────────────────────────────────────────────────────
    menu->addAction(svgIcon(QStringLiteral("folder-plus")), tr("New Folder…\tF7"),
                    [this]() { actionMkdir(); });

    // ── Открыть в терминале ───────────────────────────────────────────────────
    menu->addAction(svgIcon(QStringLiteral("terminal")),
                    tr("Open in Terminal"),
                    [path]() {
        const QString dir = QFileInfo(path).isDir() ? path : QFileInfo(path).dir().absolutePath();
        openLocalTerminal(dir);
    });

    // ── Свойства ──────────────────────────────────────────────────────────────
    if (hasSelection) {
        menu->addSeparator();
        menu->addAction(svgIcon(QStringLiteral("circle-info")), tr("Properties"),
                        [path]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
    }
}

// ── Drag & Drop ───────────────────────────────────────────────────────────────

void LocalPanel::onDropToPath(const QStringList &sourcePaths,
                              const QString     &targetPath,
                              bool               fromRemote)
{
    // Откладываем — нельзя открывать диалог или менять модель внутри dropEvent
    QTimer::singleShot(0, this, [this, sourcePaths, targetPath, fromRemote]() {
        if (fromRemote) {
            // Remote → Local: скачать
            const QString dest = targetPath.isEmpty() ? currentPath() : targetPath;
            emit downloadRequested(sourcePaths, dest);
        } else {
            // Local → Local: копирование внутри локальной ФС
            const QString dest = targetPath.isEmpty() ? currentPath() : targetPath;
            for (const QString &src : sourcePaths) {
                if (src != dest + '/' + QFileInfo(src).fileName())
                    copyRecursively(src, dest);
            }
            refresh();
        }
    });
}

// ── Фильтры ───────────────────────────────────────────────────────────────────

void LocalPanel::setShowHiddenFiles(bool show)
{
    m_showHidden = show;
    QDir::Filters f = m_model->filter();
    if (show)
        f |= QDir::Hidden;
    else
        f &= ~QDir::Hidden;
    m_model->setFilter(f);
}

// ── Сортировка ────────────────────────────────────────────────────────────────

void LocalPanel::applySortState(int column, int order)
{
    const auto ord = static_cast<Qt::SortOrder>(order);
    listView()->header()->setSortIndicator(column, ord);
    // QFileSystemModel сортируется через стандартный механизм QTreeView
    listView()->sortByColumn(column, ord);
}

int LocalPanel::sortColumn() const
{
    return listView()->header()->sortIndicatorSection();
}

int LocalPanel::sortOrder() const
{
    return static_cast<int>(listView()->header()->sortIndicatorOrder());
}

} // namespace linscp::ui::panels
