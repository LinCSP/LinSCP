#include "sftp_client.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQueue>
#include <QDirIterator>
#include <libssh/sftp.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
// MSVC sys/stat.h does not define POSIX permission bits
#  ifndef S_IRUSR
#    define S_IRUSR  0400
#    define S_IWUSR  0200
#    define S_IXUSR  0100
#    define S_IRWXU  (S_IRUSR|S_IWUSR|S_IXUSR)
#    define S_IRGRP  0040
#    define S_IWGRP  0020
#    define S_IXGRP  0010
#    define S_IROTH  0004
#    define S_IWOTH  0002
#    define S_IXOTH  0001
#  endif
#endif

namespace linscp::core::sftp {

SftpClient::SftpClient(ssh::SshSession *session, QObject *parent)
    : QObject(parent), m_session(session)
{
    init();
}

SftpClient::~SftpClient()
{
    if (m_sftp) {
        sftp_free(m_sftp);
        m_sftp = nullptr;
    }
}

bool SftpClient::init()
{
    m_sftp = sftp_new(m_session->handle());
    if (!m_sftp) {
        m_lastError = tr("Failed to create SFTP session");
        return false;
    }
    if (sftp_init(m_sftp) != SSH_OK) {
        m_lastError = tr("SFTP init error: %1").arg(sftp_get_error(m_sftp));
        sftp_free(m_sftp);
        m_sftp = nullptr;
        return false;
    }
    return true;
}

SftpDirectory SftpClient::listDirectory(const QString &remotePath)
{
    QMutexLocker locker(&m_mutex);
    SftpDirectory result;
    result.path = remotePath;

    sftp_dir dir = sftp_opendir(m_sftp, remotePath.toUtf8().constData());
    if (!dir) {
        m_lastError = tr("Cannot open directory: %1 (SFTP error %2)")
                          .arg(remotePath)
                          .arg(sftp_get_error(m_sftp));
        emit errorOccurred(m_lastError);
        result.hasError     = true;
        result.errorMessage = m_lastError;
        return result;
    }

    sftp_attributes attr;
    while ((attr = sftp_readdir(m_sftp, dir)) != nullptr) {
        const QString name = QString::fromUtf8(attr->name);
        if (name == "." || name == "..") {
            sftp_attributes_free(attr);
            continue;
        }

        SftpFileInfo info;
        info.name        = name;
        // QDir::cleanPath убирает двойные слеши (напр. при remotePath == "/")
        info.path        = QDir::cleanPath(remotePath + '/' + name);
        info.size        = static_cast<qint64>(attr->size);
        info.mtime       = QDateTime::fromSecsSinceEpoch(attr->mtime);
        info.atime       = QDateTime::fromSecsSinceEpoch(attr->atime);
        info.isDir       = (attr->type == SSH_FILEXFER_TYPE_DIRECTORY);
        info.isSymLink   = (attr->type == SSH_FILEXFER_TYPE_SYMLINK);
        info.permissions = attr->permissions;
        if (attr->owner) info.owner = QString::fromUtf8(attr->owner);
        if (attr->group) info.group = QString::fromUtf8(attr->group);

        result.entries.append(info);
        sftp_attributes_free(attr);
    }

    sftp_closedir(dir);
    return result;
}

SftpFileInfo SftpClient::stat(const QString &remotePath)
{
    QMutexLocker locker(&m_mutex);
    sftp_attributes attr = sftp_stat(m_sftp, remotePath.toUtf8().constData());
    if (!attr) return {};

    SftpFileInfo info;
    info.name        = QFileInfo(remotePath).fileName();
    info.path        = remotePath;
    info.size        = static_cast<qint64>(attr->size);
    info.mtime       = QDateTime::fromSecsSinceEpoch(attr->mtime);
    info.isDir       = (attr->type == SSH_FILEXFER_TYPE_DIRECTORY);
    info.isSymLink   = (attr->type == SSH_FILEXFER_TYPE_SYMLINK);
    info.permissions = attr->permissions;
    sftp_attributes_free(attr);
    return info;
}

bool SftpClient::downloadAsync(const QString &remotePath, const QString &localPath,
                               ProgressCallback progress)
{
    // stat() вызываем до захвата мьютекса: избегаем лишней рекурсии блокировки
    // и не держим sftp-сессию занятой в момент открытия локального файла.
    const qint64 total = stat(remotePath).size;

    QMutexLocker locker(&m_mutex);
    sftp_file remote = sftp_open(m_sftp, remotePath.toUtf8().constData(), O_RDONLY, 0);
    if (!remote) {
        m_lastError = tr("Cannot open remote file: %1").arg(remotePath);
        emit errorOccurred(m_lastError);
        return false;
    }

    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly)) {
        sftp_close(remote);
        m_lastError = tr("Cannot create local file: %1").arg(localPath);
        emit errorOccurred(m_lastError);
        return false;
    }

    // Включаем async-режим: без блокировки на read
    sftp_file_set_nonblocking(remote);
    TransferProgress tp; tp.total = total;

    // Конвейер: kAsyncWindow параллельных запросов → собираем по очереди.
    // sftp_async_read_begin/sftp_async_read помечены deprecated в libssh >= 0.10,
    // но новый sftp_aio API появился только в 0.11; используем старый со
    // явным подавлением предупреждений.
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4996)
#else
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

    struct Req { int id = -1; QByteArray buf; };
    QQueue<Req> pipeline;

    bool eof = false;
    auto issueRead = [&]() {
        if (eof) return;
        Req r;
        r.buf.resize(kChunkSize);
        r.id = sftp_async_read_begin(remote, static_cast<uint32_t>(kChunkSize));
        if (r.id < 0) { eof = true; return; }
        pipeline.enqueue(std::move(r));
    };

    // Заполняем конвейер начальными запросами
    for (int i = 0; i < kAsyncWindow; ++i) issueRead();

    bool ok = true;
    while (!pipeline.isEmpty()) {
        Req r = pipeline.dequeue();
        ssize_t nread = sftp_async_read(remote, r.buf.data(),
                                        static_cast<uint32_t>(kChunkSize),
                                        static_cast<uint32_t>(r.id));
        if (nread < 0) { ok = false; break; }
        if (nread > 0) {
            local.write(r.buf.constData(), nread);
            tp.transferred += nread;
            if (progress) progress(tp);
            issueRead(); // выдаём следующий запрос
        }
        // nread == 0 → EOF для этого слота, конвейер естественно опустеет
    }

#ifdef _MSC_VER
#  pragma warning(pop)
#else
#  pragma GCC diagnostic pop
#endif

    sftp_close(remote);
    return ok;
}

bool SftpClient::download(const QString &remotePath, const QString &localPath,
                          ProgressCallback progress)
{
    // stat() до захвата мьютекса — не держим блокировку во время двух запросов
    const qint64 total = stat(remotePath).size;

    QMutexLocker locker(&m_mutex);
    sftp_file remote = sftp_open(m_sftp, remotePath.toUtf8().constData(),
                                 O_RDONLY, 0);
    if (!remote) {
        m_lastError = tr("Cannot open remote file: %1").arg(remotePath);
        emit errorOccurred(m_lastError);
        return false;
    }

    QFile local(localPath);
    if (!local.open(QIODevice::WriteOnly)) {
        sftp_close(remote);
        m_lastError = tr("Cannot create local file: %1").arg(localPath);
        emit errorOccurred(m_lastError);
        return false;
    }
    TransferProgress tp;
    tp.total = total;

    char buf[kChunkSize];
    ssize_t nread;
    while ((nread = sftp_read(remote, buf, sizeof(buf))) > 0) {
        local.write(buf, nread);
        tp.transferred += nread;
        if (progress) progress(tp);
    }

    sftp_close(remote);
    return nread == 0; // 0 = EOF, -1 = error
}

bool SftpClient::upload(const QString &localPath, const QString &remotePath,
                        ProgressCallback progress)
{
    return uploadResume(localPath, remotePath, 0, progress);
}

bool SftpClient::uploadResume(const QString &localPath, const QString &remotePath,
                              qint64 offset, ProgressCallback progress)
{
    QMutexLocker locker(&m_mutex);
    QFile local(localPath);
    if (!local.open(QIODevice::ReadOnly)) {
        m_lastError = tr("Cannot open local file: %1").arg(localPath);
        emit errorOccurred(m_lastError);
        return false;
    }

    int flags = O_WRONLY | O_CREAT;
    if (offset > 0) flags |= O_APPEND;
    else            flags |= O_TRUNC;

    sftp_file remote = sftp_open(m_sftp, remotePath.toUtf8().constData(),
                                 flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (!remote) {
        m_lastError = tr("Cannot open remote file for writing: %1").arg(remotePath);
        emit errorOccurred(m_lastError);
        return false;
    }

    if (offset > 0) {
        local.seek(offset);
        sftp_seek64(remote, static_cast<uint64_t>(offset));
    }

    TransferProgress tp;
    tp.total       = local.size();
    tp.transferred = offset;

    char buf[kChunkSize];
    qint64 nread;
    while ((nread = local.read(buf, sizeof(buf))) > 0) {
        if (sftp_write(remote, buf, static_cast<size_t>(nread)) < 0) {
            sftp_close(remote);
            return false;
        }
        tp.transferred += nread;
        if (progress) progress(tp);
    }

    sftp_close(remote);
    return true;
}

bool SftpClient::rename(const QString &oldPath, const QString &newPath)
{
    QMutexLocker locker(&m_mutex);
    return sftp_rename(m_sftp,
                       oldPath.toUtf8().constData(),
                       newPath.toUtf8().constData()) == SSH_OK;
}

bool SftpClient::remove(const QString &remotePath)
{
    QMutexLocker locker(&m_mutex);
    return sftp_unlink(m_sftp, remotePath.toUtf8().constData()) == SSH_OK;
}

bool SftpClient::mkdir(const QString &remotePath)
{
    QMutexLocker locker(&m_mutex);
    return sftp_mkdir(m_sftp, remotePath.toUtf8().constData(),
                      S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == SSH_OK;
}

bool SftpClient::rmdir(const QString &remotePath)
{
    QMutexLocker locker(&m_mutex);
    return sftp_rmdir(m_sftp, remotePath.toUtf8().constData()) == SSH_OK;
}

bool SftpClient::uploadRecursive(const QString &localPath, const QString &remotePath,
                                  ProgressCallback progress)
{
    const QFileInfo fi(localPath);
    if (fi.isDir()) {
        if (!mkdir(remotePath)) {
            // Ошибка mkdir допустима, если директория уже существует.
            // Во всех других случаях (нет прав, нет родительского пути, …)
            // дочерние sftp_open тоже упадут — лучше сразу вернуть ошибку.
            const SftpFileInfo existing = stat(remotePath);
            if (!existing.isDir) {
                // Директория не существует И создать не удалось — настоящая ошибка.
                if (m_lastError.isEmpty())
                    m_lastError = tr("Cannot create remote directory: %1").arg(remotePath);
                emit errorOccurred(m_lastError);
                return false;
            }
        }

        for (const QString &child :
             QDir(localPath).entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            if (!uploadRecursive(localPath + '/' + child,
                                 remotePath + '/' + child, progress))
                return false;
        }
        return true;
    }
    return upload(localPath, remotePath, progress);
}

bool SftpClient::downloadRecursive(const QString &remotePath, const QString &localPath,
                                    ProgressCallback progress)
{
    const SftpFileInfo info = stat(remotePath);
    if (!info.path.isEmpty() && info.isDir) {
        // Создать локальную директорию
        QDir().mkpath(localPath);

        const SftpDirectory dir = listDirectory(remotePath);
        for (const SftpFileInfo &entry : dir.entries) {
            if (!downloadRecursive(entry.path, localPath + '/' + entry.name, progress))
                return false;
        }
        return true;
    }
    return download(remotePath, localPath, progress);
}

bool SftpClient::removeRecursive(const QString &remotePath)
{
    // Экранируем путь для shell: заменяем ' на '\''
    QString escaped = remotePath;
    escaped.replace(QStringLiteral("'"), QStringLiteral("'\\''"));
    const QString cmd = QStringLiteral("rm -rf '%1'").arg(escaped);

    // Один вызов rm -rf на сервере — мгновенно, как в WinSCP
    const QString result = m_session->execCommand(cmd);
    Q_UNUSED(result);

    // Проверяем что путь действительно исчез
    const SftpFileInfo info = stat(remotePath);
    if (info.path.isEmpty())
        return true; // удалено

    // Fallback: SFTP-рекурсия (если сервер запрещает exec)
    if (!info.isDir)
        return remove(remotePath);

    const SftpDirectory dir = listDirectory(remotePath);
    for (const SftpFileInfo &entry : dir.entries) {
        if (!removeRecursive(entry.path))
            return false;
    }
    return rmdir(remotePath);
}

bool SftpClient::chmod(const QString &remotePath, uint mode)
{
    QMutexLocker locker(&m_mutex);
    return sftp_chmod(m_sftp, remotePath.toUtf8().constData(), mode) == SSH_OK;
}

QString SftpClient::readlink(const QString &remotePath)
{
    QMutexLocker locker(&m_mutex);
    char *target = sftp_readlink(m_sftp, remotePath.toUtf8().constData());
    if (!target) return {};
    QString result = QString::fromUtf8(target);
    ssh_string_free_char(target);
    return result;
}

bool SftpClient::symlink(const QString &target, const QString &linkPath)
{
    QMutexLocker locker(&m_mutex);
    return sftp_symlink(m_sftp,
                        target.toUtf8().constData(),
                        linkPath.toUtf8().constData()) == SSH_OK;
}

} // namespace linscp::core::sftp
