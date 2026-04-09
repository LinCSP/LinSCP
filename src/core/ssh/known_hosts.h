#pragma once
#include <QString>
#include <QByteArray>

namespace linscp::core::ssh {

enum class HostVerifyResult {
    Ok,             ///< fingerprint совпадает с known_hosts
    NewHost,        ///< хост не известен — спросить пользователя
    Changed,        ///< fingerprint изменился — предупреждение!
    Error,
};

/// Управление ~/.config/linscp/known_hosts
class KnownHosts {
public:
    explicit KnownHosts(const QString &filePath);

    /// Проверить fingerprint. Вызывается из SshSession после handshake.
    HostVerifyResult verify(const QString &host, int port,
                            const QByteArray &fingerprint) const;

    /// Добавить / обновить запись (вызывается после явного accept пользователем)
    bool accept(const QString &host, int port, const QByteArray &fingerprint);

    /// Удалить запись (для случая "host changed")
    bool remove(const QString &host, int port);

private:
    QString m_filePath;
};

} // namespace linscp::core::ssh
