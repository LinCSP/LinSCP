#include "local_fs_model.h"
#include "ui/utils/file_icon_provider.h"
#include <QFileInfo>

namespace linscp::models {

LocalFsModel::LocalFsModel(QObject *parent)
    : QFileSystemModel(parent)
    , m_iconProvider(std::make_unique<linscp::FileIconProvider>())
{
    setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    setIconProvider(m_iconProvider.get());
}

QVariant LocalFsModel::data(const QModelIndex &index, int role) const
{
    if (role == FilePathRole) return filePath(index);
    if (role == IsDirRole)    return isDir(index);

    // Qt docs: QIcon must be used only in the GUI thread.
    // QFileSystemModel's FileInfoGatherer calls iconProvider()->icon() from a background
    // thread — that path returns QIcon() (empty). We supply the real icon here, where
    // data() is guaranteed to be called from the GUI thread by the view.
    if (role == Qt::DecorationRole && index.column() == 0)
        return m_iconProvider->iconForFile(QFileInfo(filePath(index)));

    return QFileSystemModel::data(index, role);
}

QVariant LocalFsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return tr("Name");
        case 1: return tr("Size");
        case 2: return tr("Type");
        case 3: return tr("Date Modified");
        }
    }
    return QFileSystemModel::headerData(section, orientation, role);
}

} // namespace linscp::models
