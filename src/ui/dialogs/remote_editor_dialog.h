#pragma once
#include <QDialog>
#include <QString>

class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace linscp::core { class IRemoteFileSystem; }

namespace linscp::ui::dialogs {

/// Простой встроенный редактор удалённого файла.
/// Скачивает содержимое файла в буфер (не на диск), показывает
/// QPlainTextEdit, при сохранении — загружает обратно через SFTP/WebDAV.
/// Аналог WinSCP Internal Editor для небольших файлов (< 4 MB).
class RemoteEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit RemoteEditorDialog(core::IRemoteFileSystem *fs,
                                const QString &remotePath,
                                QWidget *parent = nullptr);

private slots:
    void onSave();
    void onSaveAndClose();

private:
    void loadFile();
    void applyHighlighting();

    core::IRemoteFileSystem *m_fs;
    QString                 m_remotePath;

    QPlainTextEdit *m_editor  = nullptr;
    QPushButton    *m_saveBtn = nullptr;
    QLabel         *m_statusLabel = nullptr;

    bool            m_modified = false;
};

} // namespace linscp::ui::dialogs
