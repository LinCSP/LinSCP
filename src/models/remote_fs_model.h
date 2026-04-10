#pragma once
#include <QAbstractItemModel>
#include <QDateTime>
#include <QMimeDatabase>
#include <QThreadPool>
#include <QFuture>
#include <memory>

#include "core/sftp/sftp_client.h"
#include "core/sftp/sftp_file.h"

namespace linscp::models {

/// Qt item-model для SFTP файловой системы.
/// Все SFTP-запросы выполняются асинхронно через QThreadPool.
/// Кэш листингов инвалидируется через TTL или явный refresh().
class RemoteFsModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column {
        ColName = 0,
        ColSize,
        ColMtime,
        ColPermissions,
        ColOwner,
        ColCount
    };

    explicit RemoteFsModel(core::sftp::SftpClient *sftp, QObject *parent = nullptr);
    ~RemoteFsModel() override;

    void setRootPath(const QString &path);
    QString rootPath() const { return m_rootPath; }

    void setShowHidden(bool show);
    bool showHidden() const { return m_showHidden; }

    /// Сортировка в памяти — без нового SFTP-запроса.
    void setSortColumn(Column col, Qt::SortOrder order);

    /// TTL кэша листингов в секундах (0 = бесконечно).
    void setCacheTtl(int seconds) { m_cacheTtlSecs = seconds; }
    int  cacheTtl() const         { return m_cacheTtlSecs; }

    /// Принудительно перечитать директорию
    void refresh(const QModelIndex &index = {});

    core::sftp::SftpFileInfo fileInfo(const QModelIndex &index) const;
    QString                  filePath(const QModelIndex &index) const;
    bool                     isDir(const QModelIndex &index) const;

    QModelIndex indexForPath(const QString &path) const;

    // QAbstractItemModel interface
    QModelIndex   index(int row, int col, const QModelIndex &parent = {}) const override;
    QModelIndex   parent(const QModelIndex &index) const override;
    int           rowCount(const QModelIndex &parent = {}) const override;
    int           columnCount(const QModelIndex &parent = {}) const override;
    QVariant      data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant      headerData(int section, Qt::Orientation orientation,
                             int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

signals:
    void rootPathChanged(const QString &path);
    void loadingStarted(const QString &path);
    void loadingFinished(const QString &path);
    void errorOccurred(const QString &message);

private:
    struct Node;
    void  loadDirectory(Node *node);
    void  applySortToNode(Node *node);
    Node *nodeForIndex(const QModelIndex &index) const;

    core::sftp::SftpClient *m_sftp;
    QString                 m_rootPath;
    bool                    m_showHidden = false;
    Column                  m_sortCol    = ColName;
    Qt::SortOrder           m_sortOrder  = Qt::AscendingOrder;
    int                     m_cacheTtlSecs = 60; ///< TTL кэша (сек); 0 = ∞

    std::unique_ptr<Node>   m_root;
    QThreadPool             m_pool;
    int                     m_generation = 0; // инкрементируется при каждом reset

    mutable QMimeDatabase   m_mimeDb;
};

} // namespace linscp::models
