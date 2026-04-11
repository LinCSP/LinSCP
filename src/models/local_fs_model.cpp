#include "local_fs_model.h"
#include "ui/utils/file_icon_provider.h"

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
    return QFileSystemModel::data(index, role);
}

} // namespace linscp::models
