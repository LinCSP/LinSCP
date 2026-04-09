#pragma once
#include <QFileSystemModel>

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
};

} // namespace linscp::models
