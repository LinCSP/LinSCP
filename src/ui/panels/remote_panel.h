#pragma once
#include "file_panel.h"
#include <memory>

namespace linscp::core            { class IRemoteFileSystem; }
namespace linscp::core::transfer  { class TransferQueue; }
namespace linscp::core::ssh       { class SshSession; }
namespace linscp::models          { class RemoteFsModel; }

namespace linscp::ui::panels {

/// Правая панель — удалённая файловая система (SFTP, WebDAV, …)
class RemotePanel : public FilePanel {
    Q_OBJECT
public:
    explicit RemotePanel(core::IRemoteFileSystem      *fs,
                         core::transfer::TransferQueue *queue,
                         QWidget *parent = nullptr);

    /// Передать SSH-сессию для запроса свободного места (df -P).
    /// Вызывается из ConnectionTab::onSshConnected после создания панели.
    void setSshSession(core::ssh::SshSession *session) { m_sshSession = session; }

    /// Обновить путь локальной панели — используется как default в CopyDialog при Download.
    void setLocalPanelPath(const QString &path) { m_localPanelPath = path; }

    QString     currentPath() const override;
    void        navigateTo(const QString &path) override;
    void        refresh() override;
    QStringList selectedPaths() const override;

signals:
    /// Испускается при каждом изменении колонки или порядка сортировки
    void sortStateChanged(int column, int order);
    /// Испускается когда первая загрузка директории завершилась (успех или ошибка)
    void firstLoadDone();

public:
    void actionCopy()   override;   ///< F5 → Download выбранных файлов
    void actionMove()   override;   ///< F6 → Download + Delete remote
    void actionRename() override;
    void actionMkdir()  override;
    void actionDelete() override;

    void setShowHiddenFiles(bool show) override;
    bool showHiddenFiles() const override { return m_showHidden; }

    /// Восстановить сортировку (column — RemoteFsModel::Column, order — Qt::SortOrder)
    void applySortState(int column, int order);
    /// Текущая колонка сортировки (RemoteFsModel::Column)
    int  sortColumn() const;
    /// Текущий порядок сортировки (Qt::SortOrder cast to int)
    int  sortOrder() const;

    /// Показывает CopyDialog, затем ставит в очередь загрузку выбранных файлов.
    /// @param localDest  если пуст — спрашивает через CopyDialog
    /// @param isMove     если true — удалять remote-файлы после загрузки
    void downloadSelected(const QString &localDest = {}, bool isMove = false);

    /// Показывает CopyDialog, затем ставит в очередь выгрузку localPaths.
    /// @param isMove  если true — удалять локальные файлы после выгрузки
    void uploadFiles(const QStringList &localPaths, bool isMove = false);

protected:
    void onItemActivated(const QModelIndex &index) override;
    void populateContextMenu(QMenu *menu, const QModelIndex &index) override;
    void onDropToPath(const QStringList &sourcePaths,
                      const QString     &targetPath,
                      bool               fromRemote) override;

private slots:
    void onLoadingStarted(const QString &path);
    void onLoadingFinished(const QString &path);

private:
    /// Запросить свободное место через `df -P <path>`, вернуть строку "X.X GB free"
    /// или пустую строку при ошибке. Блокирующий вызов — только из фонового потока.
    QString queryFreeSpace(const QString &path) const;

    core::IRemoteFileSystem       *m_fs;
    core::transfer::TransferQueue *m_queue;
    core::ssh::SshSession         *m_sshSession        = nullptr;
    models::RemoteFsModel         *m_model             = nullptr;
    bool                           m_showHidden        = false;
    QString                        m_localPanelPath;
    bool                           m_firstLoadEmitted  = false;
};

} // namespace linscp::ui::panels
