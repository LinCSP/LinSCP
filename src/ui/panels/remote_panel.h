#pragma once
#include "file_panel.h"
#include <memory>

namespace linscp::core::sftp  { class SftpClient;  }
namespace linscp::core::transfer { class TransferQueue; }
namespace linscp::models { class RemoteFsModel; }

namespace linscp::ui::panels {

/// Правая панель — удалённая SFTP файловая система
class RemotePanel : public FilePanel {
    Q_OBJECT
public:
    explicit RemotePanel(core::sftp::SftpClient       *sftp,
                         core::transfer::TransferQueue *queue,
                         QWidget *parent = nullptr);

    QString     currentPath() const override;
    void        navigateTo(const QString &path) override;
    void        refresh() override;
    QStringList selectedPaths() const override;

    void actionCopy()   override;   ///< F5 → Download выбранных файлов
    void actionMove()   override;   ///< F6 → Download + Delete remote
    void actionRename() override;
    void actionMkdir()  override;
    void actionDelete() override;

    void setShowHiddenFiles(bool show) override;
    bool showHiddenFiles() const override { return m_showHidden; }

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
    core::sftp::SftpClient        *m_sftp;
    core::transfer::TransferQueue *m_queue;
    models::RemoteFsModel         *m_model      = nullptr;
    bool                           m_showHidden = false;
};

} // namespace linscp::ui::panels
