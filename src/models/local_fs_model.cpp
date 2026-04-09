#include "local_fs_model.h"

namespace linscp::models {

LocalFsModel::LocalFsModel(QObject *parent)
    : QFileSystemModel(parent)
{
    setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
}

QVariant LocalFsModel::data(const QModelIndex &index, int role) const
{
    if (role == FilePathRole) return filePath(index);
    if (role == IsDirRole)    return isDir(index);
    return QFileSystemModel::data(index, role);
}

} // namespace linscp::models
