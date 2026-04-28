#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QMutex>
#include <functional>
#include <libssh/sftp.h>

#include "sftp_file.h"
#include "sftp_directory.h"
#include "core/ssh/ssh_session.h"

namespace linscp::core::sftp {


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

    /// Скачать файл (remote → local), последовательное чтение
    bool download(const QString &remotePath, const QString &localPath,
                  ProgressCallback progress = {});

    /// Скачать файл с параллельными async-запросами SFTP (быстрее на высоком latency)
    bool downloadAsync(const QString &remotePath, const QString &localPath,
                       ProgressCallback progress = {});

    /// Загрузить файл (local → remote)
    bool upload(const QString &localPath, const QString &remotePath,
                ProgressCallback progress = {});

    /// Возобновить прерванную загрузку (append)
    bool uploadResume(const QString &localPath, const QString &remotePath,
                      qint64 offset, ProgressCallback progress = {});

    using SizeCallback = std::function<void(qint64)>;

    /// Загрузить файл или директорию рекурсивно (local → remote).
    /// onSizeDiscovered(bytes) вызывается при обнаружении каждого файла (до его отправки).
    bool uploadRecursive(const QString &localPath, const QString &remotePath,
                         ProgressCallback progress = {},
                         SizeCallback onSizeDiscovered = {});

    /// Скачать файл или директорию рекурсивно (remote → local).
    /// onSizeDiscovered(bytes) вызывается при обнаружении каждого нового файла —
    /// позволяет обновлять totalBytes на лету (как FileZilla).
    bool downloadRecursive(const QString &remotePath, const QString &localPath,
                           ProgressCallback progress = {},
                           SizeCallback onSizeDiscovered = {});

    /// Рекурсивно подсчитать суммарный размер файлов (в байтах)
    qint64 calcSizeRecursive(const QString &remotePath);

    bool rename(const QString &oldPath, const QString &newPath);
    bool remove(const QString &remotePath);
    bool mkdir(const QString &remotePath);
    bool rmdir(const QString &remotePath);
    /// Рекурсивно удалить файл или директорию (включая непустые)
    bool removeRecursive(const QString &remotePath);
    bool chmod(const QString &remotePath, uint mode);

    /// Прочитать цель симлинка (remote → target string)
    QString readlink(const QString &remotePath);

    /// Создать симлинк на сервере (target ← linkPath)
    bool symlink(const QString &target, const QString &linkPath);

    QString lastError() const { return m_lastError; }

signals:
    void errorOccurred(const QString &message);

private:
    bool init();

    ssh::SshSession     *m_session = nullptr;
    sftp_session         m_sftp    = nullptr;
    QString              m_lastError;
    mutable QRecursiveMutex m_mutex; ///< serialises all libssh calls (sftp_session is not thread-safe)

    // FileZilla: chunk=32KB, max in-flight=4MB (req_maxsize=4194304, sftp.c:927).
    // Окно байтовое, а не счётное — для малых файлов выдаём только нужное число запросов.
    static constexpr size_t  kChunkSize     = 32768;           ///< байт за один запрос
    static constexpr qint64  kMaxInFlight   = 4 * 1024 * 1024; ///< максимум байт в конвейере
};

} // namespace linscp::core::sftp
