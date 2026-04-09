#pragma once
#include "file_panel.h"
#include <memory>

namespace linscp::models { class LocalFsModel; }

namespace linscp::ui::panels {

/// Левая панель — локальная файловая система
class LocalPanel : public FilePanel {
    Q_OBJECT
public:
    explicit LocalPanel(QWidget *parent = nullptr);

    QString     currentPath() const override;
    void        navigateTo(const QString &path) override;
    void        refresh() override;
    QStringList selectedPaths() const override;

    void actionRename() override;
    void actionMkdir()  override;
    void actionDelete() override;

protected:
    void onItemActivated(const QModelIndex &index) override;
    void populateContextMenu(QMenu *menu, const QModelIndex &index) override;

private:
    models::LocalFsModel *m_model = nullptr;
};

} // namespace linscp::ui::panels
