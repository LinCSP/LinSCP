#include "remote_fs_model.h"
#include <QIcon>
#include <algorithm>

namespace linscp::models {

// ── Внутренняя древовидная структура ─────────────────────────────────────────

struct RemoteFsModel::Node {
    core::sftp::SftpFileInfo                 info;
    Node                                    *parent    = nullptr;
    std::vector<std::unique_ptr<Node>>       children;
    bool                                     loaded    = false;
    bool                                     loading   = false;

    int row() const {
        if (!parent) return 0;
        for (int i = 0; i < (int)parent->children.size(); ++i)
            if (parent->children[i].get() == this) return i;
        return 0;
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
    node->loaded = false;
    node->children.clear();
    emit dataChanged(index, index);
    loadDirectory(node);
}

void RemoteFsModel::loadDirectory(Node *node)
{
    if (node->loading || node->loaded) return;
    node->loading = true;
    const QString path = node->info.path;
    emit loadingStarted(path);

    const int gen = m_generation;
    m_pool.start([this, node, path, gen]() {
        auto dir = m_sftp->listDirectory(path);

        QMetaObject::invokeMethod(this, [this, node, dir = std::move(dir), gen]() mutable {
            // Если модель сбросилась пока шла загрузка — узел может быть удалён
            if (gen != m_generation) return;

            beginResetModel(); // простой способ; для больших деревьев — insertRows

            node->children.clear();
            auto entries = dir.entries;

            // Фильтрация скрытых
            if (!m_showHidden)
                entries.removeIf([](const core::sftp::SftpFileInfo &f) {
                    return f.isHidden();
                });

            // Сортировка: сначала директории, потом файлы
            std::stable_sort(entries.begin(), entries.end(),
                             [this](const core::sftp::SftpFileInfo &a,
                                    const core::sftp::SftpFileInfo &b) {
                if (a.isDir != b.isDir) return a.isDir > b.isDir;
                switch (m_sortCol) {
                case ColName:        return a.name  < b.name;
                case ColSize:        return a.size  < b.size;
                case ColMtime:       return a.mtime < b.mtime;
                case ColPermissions: return a.permissions < b.permissions;
                default: return a.name < b.name;
                }
            });

            for (auto &e : entries) {
                auto child = std::make_unique<Node>();
                child->info   = e;
                child->parent = node;
                node->children.push_back(std::move(child));
            }

            node->loaded  = true;
            node->loading = false;
            endResetModel();
            emit loadingFinished(dir.path);
        }, Qt::QueuedConnection);
    });
}

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
    // Автозагрузка только для корня — дочерние узлы загружаются через navigateTo.
    // Иначе QTreeView вызывает rowCount(child) для expand-стрелок и создаёт
    // dangling-pointer UAF при смене директории.
    if (node == m_root.get() && !node->loaded && !node->loading) {
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
        case ColPermissions: return info.permissionsString();
        case ColOwner:       return info.owner + ':' + info.group;
        }
    }
    if (role == Qt::TextAlignmentRole && index.column() == ColSize)
        return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::DecorationRole && index.column() == ColName) {
        return QIcon::fromTheme(info.isDir ? "folder" : "text-x-generic");
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
