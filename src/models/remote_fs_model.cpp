#include "remote_fs_model.h"
#include "ui/utils/file_icon_provider.h"
#include <QMimeType>
#include <algorithm>

namespace linscp::models {

// ── Внутренняя древовидная структура ─────────────────────────────────────────

struct RemoteFsModel::Node {
    core::sftp::SftpFileInfo                 info;
    Node                                    *parent    = nullptr;
    std::vector<std::unique_ptr<Node>>       children;
    bool                                     loaded    = false;
    bool                                     loading   = false;
    QDateTime                                loadedAt;  ///< время последней загрузки (TTL)

    int row() const {
        if (!parent) return 0;
        for (int i = 0; i < (int)parent->children.size(); ++i)
            if (parent->children[i].get() == this) return i;
        return 0;
    }

    bool isCacheExpired(int ttlSecs) const {
        if (!loaded) return true;
        if (ttlSecs == 0) return false;
        return loadedAt.secsTo(QDateTime::currentDateTime()) > ttlSecs;
    }
};

// ─────────────────────────────────────────────────────────────────────────────

RemoteFsModel::RemoteFsModel(core::sftp::SftpClient *sftp, QObject *parent)
    : QAbstractItemModel(parent), m_sftp(sftp)
{
    m_pool.setMaxThreadCount(1); // sftp_session не thread-safe — только 1 поток
    m_root = std::make_unique<Node>();
    m_root->info.isDir = true;
}

RemoteFsModel::~RemoteFsModel() = default;

void RemoteFsModel::setRootPath(const QString &path)
{
    ++m_generation;
    beginResetModel();
    m_rootPath = path;
    m_root->info.path = path;
    m_root->children.clear();
    m_root->loaded  = false;
    m_root->loading = false;
    endResetModel();

    loadDirectory(m_root.get());
}

void RemoteFsModel::setShowHidden(bool show)
{
    if (m_showHidden == show) return;
    m_showHidden = show;
    refresh();
}

void RemoteFsModel::refresh(const QModelIndex &index)
{
    ++m_generation;
    Node *node = index.isValid() ? nodeForIndex(index) : m_root.get();
    if (!node) return;

    // ВАЖНО: beginResetModel/endResetModel обязательны перед очисткой children.
    // Иначе QTreeView удерживает QModelIndex::internalPointer на уже удалённые
    // узлы и крашится при следующем paintEvent в parent().
    beginResetModel();
    node->children.clear();
    node->loaded  = false;
    node->loading = false;
    endResetModel();

    loadDirectory(node);
}

// ── Сортировка в памяти ───────────────────────────────────────────────────────

void RemoteFsModel::setSortColumn(Column col, Qt::SortOrder order)
{
    if (m_sortCol == col && m_sortOrder == order) return;
    m_sortCol   = col;
    m_sortOrder = order;

    if (!m_root->loaded || m_root->children.empty()) return;

    // Сохраняем persistent-индексы ДО перестановки
    const QModelIndexList oldPersistent = persistentIndexList();

    emit layoutAboutToBeChanged();

    applySortToNode(m_root.get());

    // Пересчитываем persistent-индексы ПОСЛЕ перестановки.
    // Без этого QTreeView держит индексы со старыми номерами строк.
    // Поиск идёт по internalPointer (Node*), который не меняется при сортировке.
    QModelIndexList newPersistent;
    newPersistent.reserve(oldPersistent.size());
    for (const QModelIndex &oldIdx : oldPersistent) {
        if (!oldIdx.isValid()) { newPersistent << QModelIndex{}; continue; }
        auto *n = static_cast<Node *>(oldIdx.internalPointer());
        Node *p = n->parent ? n->parent : m_root.get();
        int newRow = -1;
        for (int i = 0; i < (int)p->children.size(); ++i) {
            if (p->children[i].get() == n) { newRow = i; break; }
        }
        newPersistent << (newRow >= 0
            ? createIndex(newRow, oldIdx.column(), n)
            : QModelIndex{});
    }
    changePersistentIndexList(oldPersistent, newPersistent);

    emit layoutChanged();
}

void RemoteFsModel::applySortToNode(Node *node)
{
    if (node->children.empty()) return;

    const bool asc = (m_sortOrder == Qt::AscendingOrder);

    std::stable_sort(node->children.begin(), node->children.end(),
        [&](const std::unique_ptr<Node> &a, const std::unique_ptr<Node> &b) {
            // Директории первые при ascending, последние при descending
            if (a->info.isDir != b->info.isDir)
                return asc ? (a->info.isDir > b->info.isDir)
                           : (a->info.isDir < b->info.isDir);

            bool less = false;
            switch (m_sortCol) {
            case ColName:        less = a->info.name  < b->info.name;  break;
            case ColSize:        less = a->info.size  < b->info.size;  break;
            case ColMtime:       less = a->info.mtime < b->info.mtime; break;
            case ColPermissions: less = a->info.permissions < b->info.permissions; break;
            case ColOwner:       less = a->info.owner < b->info.owner; break;
            default:             less = a->info.name  < b->info.name;  break;
            }
            return asc ? less : !less;
        });

    // Рекурсивно для поддиректорий
    for (auto &child : node->children)
        if (child->info.isDir && child->loaded)
            applySortToNode(child.get());
}

// ── Загрузка директории ───────────────────────────────────────────────────────

void RemoteFsModel::loadDirectory(Node *node)
{
    if (node->loading) return;
    // Пропустить если кэш ещё свежий
    if (node->loaded && !node->isCacheExpired(m_cacheTtlSecs)) return;

    node->loading = true;
    const QString path = node->info.path;
    emit loadingStarted(path);

    const int gen = m_generation;
    m_pool.start([this, node, path, gen]() {
        auto dir = m_sftp->listDirectory(path);

        QMetaObject::invokeMethod(this, [this, node, dir = std::move(dir), gen]() mutable {
            if (gen != m_generation) return;

            if (dir.hasError) {
                // Листинг не удался (сессия оборвана, нет прав, …).
                // Вызываем begin/endResetModel чтобы не оставить модель в незавершённом
                // состоянии (beginResetModel уже вызван в refresh()), и сигналим ошибку.
                node->loading = false;
                // НЕ помечаем loaded=true — следующий вручную вызванный refresh()
                // сделает ещё одну попытку загрузить директорию.
                beginResetModel();
                node->children.clear();
                endResetModel();
                emit errorOccurred(dir.errorMessage);
                return;
            }

            beginResetModel();

            node->children.clear();
            auto entries = dir.entries;

            // Фильтрация скрытых
            if (!m_showHidden)
                entries.removeIf([](const core::sftp::SftpFileInfo &f) {
                    return f.isHidden();
                });

            // Сортировка
            const bool asc = (m_sortOrder == Qt::AscendingOrder);
            std::stable_sort(entries.begin(), entries.end(),
                [&](const core::sftp::SftpFileInfo &a, const core::sftp::SftpFileInfo &b) {
                    if (a.isDir != b.isDir)
                        return asc ? (a.isDir > b.isDir) : (a.isDir < b.isDir);
                    bool less = false;
                    switch (m_sortCol) {
                    case ColName:        less = a.name  < b.name;  break;
                    case ColSize:        less = a.size  < b.size;  break;
                    case ColMtime:       less = a.mtime < b.mtime; break;
                    case ColPermissions: less = a.permissions < b.permissions; break;
                    case ColOwner:       less = a.owner < b.owner; break;
                    default:             less = a.name  < b.name;  break;
                    }
                    return asc ? less : !less;
                });

            for (auto &e : entries) {
                auto child = std::make_unique<Node>();
                child->info   = e;
                child->parent = node;
                node->children.push_back(std::move(child));
            }

            node->loaded   = true;
            node->loading  = false;
            node->loadedAt = QDateTime::currentDateTime();
            endResetModel();
            emit loadingFinished(dir.path);
        }, Qt::QueuedConnection);
    });
}

// ── QAbstractItemModel ────────────────────────────────────────────────────────

RemoteFsModel::Node *RemoteFsModel::nodeForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) return m_root.get();
    return static_cast<Node *>(index.internalPointer());
}

QModelIndex RemoteFsModel::index(int row, int col, const QModelIndex &parent) const
{
    Node *parentNode = nodeForIndex(parent);
    if (row < 0 || row >= (int)parentNode->children.size()) return {};
    return createIndex(row, col, parentNode->children[row].get());
}

QModelIndex RemoteFsModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return {};
    Node *node = nodeForIndex(index);
    Node *p    = node->parent;
    if (!p || p == m_root.get()) return {};
    return createIndex(p->row(), 0, p);
}

int RemoteFsModel::rowCount(const QModelIndex &parent) const
{
    Node *node = nodeForIndex(parent);
    // Автозагрузка только для корня
    if (node == m_root.get() && !node->loaded && !node->loading) {
        const_cast<RemoteFsModel *>(this)->loadDirectory(node);
    } else if (node == m_root.get() && node->isCacheExpired(m_cacheTtlSecs)) {
        // Кэш устарел — перезагрузить в фоне (без сброса текущих данных)
        const_cast<RemoteFsModel *>(this)->loadDirectory(node);
    }
    return static_cast<int>(node->children.size());
}

int RemoteFsModel::columnCount(const QModelIndex &) const { return ColCount; }

QVariant RemoteFsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    const Node *node = nodeForIndex(index);
    const auto &info = node->info;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColName:        return info.name;
        case ColSize: {
            if (info.isDir) return QVariant{};
            constexpr qint64 KB = 1024, MB = 1024 * 1024, GB = 1024 * 1024 * 1024;
            if (info.size < KB)  return tr("%1 B").arg(info.size);
            if (info.size < MB)  return tr("%1 KB").arg(info.size / KB);
            if (info.size < GB)  return tr("%1 MB").arg(info.size / MB);
            return tr("%1 GB").arg(info.size / GB);
        }
        case ColMtime:       return info.mtime.toString("dd.MM.yyyy HH:mm");
        case ColPermissions: return QString("%1  %2")
                                 .arg(info.permissionsString())
                                 .arg(info.permissions & 07777, 4, 8, QChar('0'));
        case ColOwner:       return info.owner.isEmpty()
                                 ? info.group
                                 : info.owner + ':' + info.group;
        }
    }

    if (role == Qt::TextAlignmentRole && index.column() == ColSize)
        return int(Qt::AlignRight | Qt::AlignVCenter);

    if (role == Qt::DecorationRole && index.column() == ColName) {
        if (info.isDir)
            return svgFolderIcon();
        if (info.isSymLink)
            return svgIcon(QStringLiteral("link"));
        const QMimeType mime = m_mimeDb.mimeTypeForFile(
            info.name, QMimeDatabase::MatchExtension);
        return linscp::iconForMime(mime);
    }

    if (role == Qt::ToolTipRole) {
        if (index.column() == ColPermissions) {
            // Подробный tooltip: символьная + octal
            return QString("%1\noctal: %2")
                .arg(info.permissionsString())
                .arg(info.permissions & 07777, 4, 8, QChar('0'));
        }
        if (index.column() == ColName && info.isSymLink) {
            return tr("Symlink → %1").arg(info.linkTarget);
        }
    }

    return {};
}

QVariant RemoteFsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case ColName:        return tr("Name");
    case ColSize:        return tr("Size");
    case ColMtime:       return tr("Modified");
    case ColPermissions: return tr("Permissions");
    case ColOwner:       return tr("Owner");
    }
    return {};
}

Qt::ItemFlags RemoteFsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

core::sftp::SftpFileInfo RemoteFsModel::fileInfo(const QModelIndex &index) const
{
    if (!index.isValid()) return {};
    return nodeForIndex(index)->info;
}

QString RemoteFsModel::filePath(const QModelIndex &index) const
{
    return fileInfo(index).path;
}

bool RemoteFsModel::isDir(const QModelIndex &index) const
{
    return fileInfo(index).isDir;
}

QModelIndex RemoteFsModel::indexForPath(const QString &path) const
{
    std::function<QModelIndex(Node *, const QString &)> find;
    find = [&](Node *node, const QString &p) -> QModelIndex {
        for (int i = 0; i < (int)node->children.size(); ++i) {
            Node *child = node->children[i].get();
            if (child->info.path == p)
                return createIndex(i, 0, child);
            if (p.startsWith(child->info.path + '/')) {
                auto sub = find(child, p);
                if (sub.isValid()) return sub;
            }
        }
        return {};
    };
    return find(m_root.get(), path);
}

} // namespace linscp::models
