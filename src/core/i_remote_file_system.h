#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include "core/sftp/sftp_file.h"

namespace linscp::core {

/// Абстрактный интерфейс удалённой файловой системы.
/// Аналог WinSCP TCustomFileSystem.
/// Все методы синхронные — вызывать из воркер-потока (QThreadPool / QFuture).
class IRemoteFileSystem : public QObject {
    Q_OBJECT
public:
    explicit IRemoteFileSystem(QObject *parent = nullptr) : QObject(parent) {}
    ~IRemoteFileSystem() override = default;

    // ── Навигация ─────────────────────────────────────────────────────────────
    virtual QList<sftp::SftpFileInfo> list(const QString &path) = 0;
    virtual sftp::SftpFileInfo        stat(const QString &path) = 0;

    // ── Передача ──────────────────────────────────────────────────────────────
    virtual bool download(const QString &remotePath, const QString &localPath,
                          sftp::ProgressCallback progress = {}) = 0;
    virtual bool upload(const QString &localPath, const QString &remotePath,
                        sftp::ProgressCallback progress = {}) = 0;
    virtual bool uploadResume(const QString &localPath, const QString &remotePath,
                              qint64 offset, sftp::ProgressCallback progress = {}) = 0;
    virtual bool uploadRecursive(const QString &localPath, const QString &remotePath,
                                 sftp::ProgressCallback progress = {}) = 0;
    virtual bool downloadRecursive(const QString &remotePath, const QString &localPath,
                                   sftp::ProgressCallback progress = {}) = 0;

    // ── Файловые операции ─────────────────────────────────────────────────────
    virtual bool rename(const QString &oldPath, const QString &newPath) = 0;
    virtual bool remove(const QString &remotePath) = 0;
    virtual bool mkdir(const QString &remotePath) = 0;
    virtual bool rmdir(const QString &remotePath) { return remove(remotePath); }
    virtual bool chmod(const QString &remotePath, uint mode)
        { Q_UNUSED(remotePath); Q_UNUSED(mode); return false; }
    virtual QString readlink(const QString &remotePath)
        { Q_UNUSED(remotePath); return {}; }
    virtual bool symlink(const QString &target, const QString &linkPath)
        { Q_UNUSED(target); Q_UNUSED(linkPath); return false; }
    virtual bool removeRecursive(const QString &remotePath)
        { return remove(remotePath); }

    // ── Возможности протокола ─────────────────────────────────────────────────
    virtual bool supportsChmod()     const { return false; }
    virtual bool supportsSymlinks()  const { return false; }
    virtual bool supportsOwner()     const { return false; }
    virtual bool supportsFreeSpace() const { return false; }

    /// Свободное место в байтах; -1 если не поддерживается или ошибка
    virtual qint64 freeSpace(const QString &path) { Q_UNUSED(path); return -1; }

    virtual QString lastError() const = 0;

signals:
    void errorOccurred(const QString &message);
};

} // namespace linscp::core
