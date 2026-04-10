#pragma once
#include <QDialog>
#include <QDateTime>

class QLabel;
class QPushButton;
class QCheckBox;

namespace linscp::ui::dialogs {

/// Диалог замены файла при копировании/перемещении.
/// Аналог TCopyParamType::OnTransferExists в WinSCP.
///
/// Показывается когда целевой файл уже существует.
/// Возвращает решение: заменить, пропустить, переименовать или отмену.
class OverwriteDialog : public QDialog {
    Q_OBJECT
public:
    enum class Action {
        Overwrite,      ///< Заменить этот файл
        OverwriteAll,   ///< Заменить все
        Skip,           ///< Пропустить этот файл
        SkipAll,        ///< Пропустить все
        Rename,         ///< Переименовать целевой файл
        Cancel,         ///< Отменить всю операцию
    };

    struct FileInfo {
        QString   path;
        qint64    size     = -1;     ///< -1 = неизвестно
        QDateTime modified;
    };

    explicit OverwriteDialog(const FileInfo &source,
                             const FileInfo &target,
                             QWidget *parent = nullptr);

    Action     action()      const { return m_action;  }
    QString    newName()     const { return m_newName; }

private slots:
    void onOverwrite();
    void onOverwriteAll();
    void onSkip();
    void onSkipAll();
    void onRename();
    void onCancel();

private:
    void setupUi(const FileInfo &source, const FileInfo &target);
    static QString formatInfo(const FileInfo &fi);

    Action  m_action  = Action::Cancel;
    QString m_newName;
};

} // namespace linscp::ui::dialogs
