#include "file_utils.h"
#include <QRegularExpression>

namespace linscp::utils {

QString FileUtils::humanSize(qint64 bytes)
{
    if (bytes < 0)            return QStringLiteral("?");
    if (bytes < 1024)         return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)  return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 2);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

QString FileUtils::joinPath(const QString &base, const QString &name)
{
    if (base.endsWith('/')) return normalizePath(base + name);
    return normalizePath(base + '/' + name);
}

QString FileUtils::normalizePath(const QString &path)
{
    QString result = path;
    // Убрать двойные слэши
    static const QRegularExpression re(QStringLiteral("/{2,}"));
    result.replace(re, QStringLiteral("/"));
    // Убрать trailing slash (кроме корня)
    if (result.size() > 1 && result.endsWith('/'))
        result.chop(1);
    return result;
}

QString FileUtils::fileName(const QString &path)
{
    const int idx = path.lastIndexOf('/');
    return idx >= 0 ? path.mid(idx + 1) : path;
}

QString FileUtils::dirName(const QString &path)
{
    const int idx = path.lastIndexOf('/');
    if (idx <= 0) return QStringLiteral("/");
    return path.left(idx);
}

bool FileUtils::matchWildcard(const QString &pattern, const QString &name)
{
    const QRegularExpression re(
        QRegularExpression::wildcardToRegularExpression(pattern),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(name).hasMatch();
}

} // namespace linscp::utils
