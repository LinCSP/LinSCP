#pragma once
#include <QString>
#include <QStringList>

namespace linscp::utils {

class FileUtils {
public:
    /// Человекочитаемый размер: "1.23 MB", "45 KB", ...
    static QString humanSize(qint64 bytes);

    /// Объединить путь (аналог QDir::cleanPath но для POSIX-путей)
    static QString joinPath(const QString &base, const QString &name);

    /// Нормализовать POSIX-путь (убрать двойные слэши, .)
    static QString normalizePath(const QString &path);

    /// Вернуть имя файла (часть после последнего '/')
    static QString fileName(const QString &path);

    /// Вернуть директорию (часть до последнего '/')
    static QString dirName(const QString &path);

    /// Проверить glob-маску
    static bool matchWildcard(const QString &pattern, const QString &name);
};

} // namespace linscp::utils
