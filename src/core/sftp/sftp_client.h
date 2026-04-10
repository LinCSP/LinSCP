#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <functional>
#include <libssh/sftp.h>

#include "sftp_file.h"
#include "sftp_directory.h"
#include "core/ssh/ssh_session.h"

namespace linscp::core::sftp {

/// Прогресс передачи (байты передано / всего)
struct TransferProgress {
    qint64 transferred = 0;
    qint64 total       = 0;
    int    percent() const {
        return total > 0 ? static_cast<int>(transferred * 100 / total) : 0;
    }
};

using ProgressCallback = std::function<void(const TransferProgress &)>;

/// SFTP-клиент поверх SshSession.
/// Все методы синхронные — вызывать из воркер-потока (QThreadPool / QFuture).
class SftpClient : public QObject {
    Q_OBJECT
public:
    explicit SftpClient(ssh::SshSession *session, QObject *parent = nullptr);
    ~SftpClient() override;

    bool isConnected() const { return m_sftp != nullptr; }

    // ── Навигация ─────────────────────────────────────────────────────────────

    /// Получить листинг директории
    SftpDirectory listDirectory(const QString &remotePath);

    /// Статистика одного файла
    SftpFileInfo  stat(const QString &remotePath);

    // ── Файловые операции ─────────────────────────────────────────────────────

    /// Скачать файл (remote → local)
    bool download(const QString &remotePath, const QString &localPath,
                  ProgressCallback progress = {});

    /// Загрузить файл (local → remote)
    bool upload(const QString &localPath, const QString &remotePath,
                ProgressCallback progress = {});

    /// Возобновить прерванную загрузку (append)
    bool uploadResume(const QString &localPath, const QString &remotePath,
                      qint64 offset, ProgressCallback progress = {});

    /// Загрузить файл или директорию рекурсивно (local → remote)
    bool uploadRecursive(const QString &localPath, const QString &remotePath,
                         ProgressCallback progress = {});

    /// Скачать файл или директорию рекурсивно (remote → local)
    bool downloadRecursive(const QString &remotePath, const QString &localPath,
                           ProgressCallback progress = {});

    bool rename(const QString &oldPath, const QString &newPath);
    bool remove(const QString &remotePath);
    bool mkdir(const QString &remotePath);
    bool rmdir(const QString &remotePath);
    /// Рекурсивно удалить файл или директорию (включая непустые)
    bool removeRecursive(const QString &remotePath);
    bool chmod(const QString &remotePath, uint mode);

    QString lastError() const { return m_lastError; }

signals:
    void errorOccurred(const QString &message);

private:
    bool init();

    ssh::SshSession *m_session = nullptr;
    sftp_session     m_sftp    = nullptr;
    QString          m_lastError;

    static constexpr qint64 kChunkSize = 32768; ///< байт за одну операцию чтения/записи
};

} // namespace linscp::core::sftp
