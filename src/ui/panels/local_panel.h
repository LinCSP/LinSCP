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

    void actionCopy()   override;   ///< F5 → Upload выбранных файлов на remote
    void actionMove()   override;   ///< F6 → Upload + Delete local
    void actionRename() override;
    void actionMkdir()  override;
    void actionDelete() override;

    void setShowHiddenFiles(bool show) override;
    bool showHiddenFiles() const override { return m_showHidden; }

    /// Восстановить сортировку (column — индекс, order — Qt::SortOrder as int)
    void applySortState(int column, int order);
    int  sortColumn() const;
    int  sortOrder()  const;

signals:
    void sortStateChanged(int column, int order);

protected:
    void onItemActivated(const QModelIndex &index) override;
    void populateContextMenu(QMenu *menu, const QModelIndex &index) override;
    void onDropToPath(const QStringList &sourcePaths,
                      const QString     &targetPath,
                      bool               fromRemote) override;

private:
    models::LocalFsModel *m_model      = nullptr;
    bool                  m_showHidden = false;
};

} // namespace linscp::ui::panels
