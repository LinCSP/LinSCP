#pragma once
#include <QDialog>
#include <QString>

class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace linscp::core::sftp { class SftpClient; }

namespace linscp::ui::dialogs {

/// Простой встроенный редактор удалённого файла.
/// Скачивает содержимое файла в буфер (не на диск), показывает
/// QPlainTextEdit, при сохранении — загружает обратно через SFTP.
/// Аналог WinSCP Internal Editor для небольших файлов (< 4 MB).
class RemoteEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit RemoteEditorDialog(core::sftp::SftpClient *sftp,
                                const QString &remotePath,
                                QWidget *parent = nullptr);

private slots:
    void onSave();
    void onSaveAndClose();

private:
    void loadFile();
    void applyHighlighting();

    core::sftp::SftpClient *m_sftp;
    QString                 m_remotePath;

    QPlainTextEdit *m_editor  = nullptr;
    QPushButton    *m_saveBtn = nullptr;
    QLabel         *m_statusLabel = nullptr;

    bool            m_modified = false;
};

} // namespace linscp::ui::dialogs
