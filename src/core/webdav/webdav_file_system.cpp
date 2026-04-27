#include "webdav_file_system.h"
#include "core/session/session_profile.h"

#include <QDir>
#include <QFileInfo>
#include <QStack>

namespace linscp::core::webdav {

// ── Конструкторы ──────────────────────────────────────────────────────────────

WebDavFileSystem::WebDavFileSystem(const session::SessionProfile &profile,
                                   QObject *parent)
    : IRemoteFileSystem(parent)
    , m_client(new WebDavClient(profile.webDavEncryption,
                                profile.host, profile.port,
                                profile.username, profile.password,
                                this))
{
}

WebDavFileSystem::WebDavFileSystem(WebDavEncryption enc,
                                   const QString &host, quint16 port,
                                   const QString &username, const QString &password,
                                   QObject *parent)
    : IRemoteFileSystem(parent)
    , m_client(new WebDavClient(enc, host, port, username, password, this))
{
}

bool WebDavFileSystem::isConnected() const
{
    return m_client->isConnected();
}

// ── Листинг ───────────────────────────────────────────────────────────────────

QList<sftp::SftpFileInfo> WebDavFileSystem::list(const QString &path)
{
    const auto entries = m_client->propfind(path, 1);
    if (entries.isEmpty() && !m_client->lastError().isEmpty()) {
        emit errorOccurred(m_client->lastError());
        return {};
    }

    QList<sftp::SftpFileInfo> result;
    result.reserve(entries.size());

    // Первый элемент — сама директория; пропускаем её
    bool first = true;
    for (const WebDavFileInfo &dav : entries) {
        if (first) { first = false; continue; }
        result.append(toSftpInfo(dav, path));
    }
    return result;
}

sftp::SftpFileInfo WebDavFileSystem::stat(const QString &path)
{
    const auto entries = m_client->propfind(path, 0);
    if (entries.isEmpty()) return {};
    return toSftpInfo(entries.first(), path.section('/', 0, -2));
}

// ── Передача ─────────────────────────────────────────────────────────────────

bool WebDavFileSystem::download(const QString &remotePath, const QString &localPath,
                                sftp::ProgressCallback progress)
{
    return m_client->get(remotePath, localPath, 0, std::move(progress));
}

bool WebDavFileSystem::upload(const QString &localPath, const QString &remotePath,
                              sftp::ProgressCallback progress)
{
    return m_client->put(localPath, remotePath, 0, std::move(progress));
}

bool WebDavFileSystem::uploadResume(const QString &localPath, const QString &remotePath,
                                    qint64 offset, sftp::ProgressCallback progress)
{
    return m_client->put(localPath, remotePath, offset, std::move(progress));
}

bool WebDavFileSystem::uploadRecursive(const QString &localPath, const QString &remotePath,
                                       sftp::ProgressCallback progress)
{
    const QFileInfo fi(localPath);
    if (!fi.isDir())
        return upload(localPath, remotePath, progress);

    // Рекурсивный обход локального дерева
    const QString destBase = remotePath + '/' + fi.fileName();
    if (!mkdir(destBase)) {
        // 405 — папка уже существует, это нормально; иная ошибка — прерываем
        const sftp::SftpFileInfo check = stat(destBase);
        if (check.path.isEmpty() || !check.isDir)
            return false;
    }

    const QDir dir(localPath);
    for (const QFileInfo &entry : dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        const QString destPath = destBase + '/' + entry.fileName();
        if (entry.isDir()) {
            if (!uploadRecursive(entry.filePath(), destBase, progress))
                return false;
        } else {
            if (!upload(entry.filePath(), destPath, progress))
                return false;
        }
    }
    return true;
}

bool WebDavFileSystem::downloadRecursive(const QString &remotePath, const QString &localPath,
                                         sftp::ProgressCallback progress,
                                         SizeCallback onSizeDiscovered)
{
    const sftp::SftpFileInfo info = stat(remotePath);
    if (info.path.isEmpty()) return false;

    if (!info.isDir) {
        if (onSizeDiscovered) onSizeDiscovered(info.size);
        return download(remotePath, localPath + '/' + info.name, progress);
    }

    // Рекурсивный обход удалённого дерева
    const QString destBase = localPath + '/' + info.name;
    QDir().mkpath(destBase);

    const QList<sftp::SftpFileInfo> entries = list(remotePath);
    if (onSizeDiscovered) {
        for (const sftp::SftpFileInfo &entry : entries)
            if (!entry.isDir) onSizeDiscovered(entry.size);
    }
    for (const sftp::SftpFileInfo &entry : entries) {
        if (entry.isDir) {
            if (!downloadRecursive(entry.path, destBase, progress, onSizeDiscovered))
                return false;
        } else {
            if (!download(entry.path, destBase + '/' + entry.name, progress))
                return false;
        }
    }
    return true;
}

// ── Файловые операции ─────────────────────────────────────────────────────────

bool WebDavFileSystem::rename(const QString &oldPath, const QString &newPath)
{
    return m_client->move(oldPath, newPath, true);
}

bool WebDavFileSystem::remove(const QString &remotePath)
{
    return m_client->del(remotePath);
}

bool WebDavFileSystem::mkdir(const QString &remotePath)
{
    return m_client->mkcol(remotePath);
}

bool WebDavFileSystem::removeRecursive(const QString &remotePath)
{
    // WebDAV DELETE рекурсивно удаляет коллекцию на сервере
    return m_client->del(remotePath);
}

// ── Свободное место ───────────────────────────────────────────────────────────

qint64 WebDavFileSystem::freeSpace(const QString &path)
{
    const auto entries = m_client->propfind(path, 0);
    if (entries.isEmpty()) return -1;
    return entries.first().quotaAvailable;
}

// ── Ошибка ────────────────────────────────────────────────────────────────────

QString WebDavFileSystem::lastError() const
{
    return m_client->lastError();
}

// ── Конвертация WebDavFileInfo → SftpFileInfo ─────────────────────────────────

sftp::SftpFileInfo WebDavFileSystem::toSftpInfo(const WebDavFileInfo &dav,
                                                 const QString &parentPath)
{
    sftp::SftpFileInfo info;

    // Имя файла: последний сегмент href (без завершающего '/')
    QString href = dav.href;
    if (href.endsWith('/')) href.chop(1);
    info.name  = href.section('/', -1);
    info.path  = parentPath.endsWith('/')
                     ? parentPath + info.name
                     : parentPath + '/' + info.name;

    if (dav.isCollection) {
        info.isDir = true;
        info.path += '/';
    }

    info.size  = dav.contentLength;
    info.mtime = dav.lastModified;
    info.atime = dav.lastModified;

    // WebDAV не предоставляет Unix-права и владельца
    info.permissions = dav.isCollection ? 0755 : 0644;
    info.owner.clear();
    info.group.clear();

    // displayName переопределяет имя только если оно не пустое
    if (!dav.displayName.isEmpty() && dav.displayName != info.name)
        info.name = dav.displayName;

    return info;
}

} // namespace linscp::core::webdav
