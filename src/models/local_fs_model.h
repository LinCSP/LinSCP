#pragma once
#include "ui/utils/file_icon_provider.h"
#include <QFileSystemModel>
#include <memory>

namespace linscp::models {

/// Расширение QFileSystemModel: скрытые файлы, дополнительные роли
class LocalFsModel : public QFileSystemModel {
    Q_OBJECT
public:
    explicit LocalFsModel(QObject *parent = nullptr);

    // Дополнительные роли
    enum {
        FilePathRole  = Qt::UserRole + 1,
        IsDirRole,
    };

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    std::unique_ptr<linscp::FileIconProvider> m_iconProvider;
};

} // namespace linscp::models
