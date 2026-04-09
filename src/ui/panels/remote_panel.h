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
    explicit RemotePanel(core::sftp::SftpClient    *sftp,
                         core::transfer::TransferQueue *queue,
                         QWidget *parent = nullptr);

    QString     currentPath() const override;
    void        navigateTo(const QString &path) override;
    void        refresh() override;
    QStringList selectedPaths() const override;

    void actionRename() override;
    void actionMkdir()  override;
    void actionDelete() override;

    void downloadSelected(const QString &localDest);
    void uploadFiles(const QStringList &localPaths);

protected:
    void onItemActivated(const QModelIndex &index) override;
    void populateContextMenu(QMenu *menu, const QModelIndex &index) override;

private slots:
    void onLoadingStarted(const QString &path);
    void onLoadingFinished(const QString &path);

private:
    core::sftp::SftpClient        *m_sftp;
    core::transfer::TransferQueue *m_queue;
    models::RemoteFsModel         *m_model = nullptr;
};

} // namespace linscp::ui::panels
