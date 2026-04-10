#pragma once
#include <QDialog>
#include <QStringList>

class QLabel;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QGroupBox;

namespace linscp::ui::dialogs {

/// Диалог копирования/перемещения файлов — аналог TCopyDialog в WinSCP.
/// Показывается при F5 (копирование) и F6 (перемещение) между панелями,
/// а также при drag&drop и контекстном меню Download/Upload.
class CopyDialog : public QDialog {
    Q_OBJECT
public:
    enum class Direction { Upload, Download };

    explicit CopyDialog(Direction direction,
                        bool isMove,
                        const QStringList &fileList,
                        const QString &defaultTarget,
                        QWidget *parent = nullptr);

    /// Целевая директория (после закрытия диалога)
    QString targetDirectory() const;

    /// Пользователь нажал "Queue" вместо "Copy"
    bool addToQueue() const { return m_addToQueue; }

private slots:
    void onBrowse();
    void onOk();

private:
    void setupUi(const QString &defaultTarget);

    Direction    m_direction;
    bool         m_isMove;
    QStringList  m_fileList;
    bool         m_addToQueue = false;

    QLabel      *m_filesLabel   = nullptr;
    QLabel      *m_dirLabel     = nullptr;
    QLineEdit   *m_targetEdit   = nullptr;
    QPushButton *m_browseBtn    = nullptr;
    QCheckBox   *m_queueCheck   = nullptr;
    QPushButton *m_okBtn        = nullptr;
    QPushButton *m_cancelBtn    = nullptr;
};

} // namespace linscp::ui::dialogs
