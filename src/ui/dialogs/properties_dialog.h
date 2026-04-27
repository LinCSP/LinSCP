#pragma once
#include <QDialog>
#include "core/sftp/sftp_file.h"

class QLabel;
class QLineEdit;
class QCheckBox;
class QGroupBox;
class QTabWidget;

namespace linscp::core { class IRemoteFileSystem; }

namespace linscp::ui::dialogs {

/// Диалог свойств файла/директории — аналог TPropertiesDialog в WinSCP.
/// Показывает имя, путь, размер, дату, владельца, права (chmod).
class PropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PropertiesDialog(const core::sftp::SftpFileInfo &fileInfo,
                              core::IRemoteFileSystem *fs,
                              QWidget *parent = nullptr);

private slots:
    void onApply();
    void onPermToggled();

private:
    void setupUi();
    void populateInfo();
    uint collectPermissions() const;

    core::sftp::SftpFileInfo  m_info;
    core::IRemoteFileSystem  *m_fs;

    // Общая вкладка
    QLineEdit *m_nameEdit    = nullptr;
    QLabel    *m_locationLbl = nullptr;
    QLabel    *m_sizeLbl     = nullptr;
    QLabel    *m_dateLbl     = nullptr;
    QLabel    *m_ownerLbl    = nullptr;
    QLabel    *m_groupLbl    = nullptr;

    // Права доступа (3×3 checkboxes: owner/group/other × read/write/exec)
    QCheckBox *m_perm[3][3]  = {};   ///< [ugо][rwx]
    QCheckBox *m_stickyChk   = nullptr;
    QLabel    *m_octalLbl    = nullptr;

    QPushButton *m_applyBtn  = nullptr;
};

} // namespace linscp::ui::dialogs
