#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <libssh/libssh.h>

namespace linscp::core::keys {

enum class KeyType { RSA, ED25519, ECDSA, Unknown };

struct SshKeyInfo {
    QString  path;
    KeyType  type      = KeyType::Unknown;
    int      bits      = 0;      ///< для RSA: 2048/4096
    QString  comment;
    QString  fingerprint;        ///< SHA256:...
    bool     hasPassphrase = false;
};

/// Загрузка, кэширование passphrase, определение типа ключей
class KeyManager : public QObject {
    Q_OBJECT
public:
    explicit KeyManager(QObject *parent = nullptr);
    ~KeyManager() override;

    /// Прочитать метаданные ключа без загрузки в память
    SshKeyInfo inspect(const QString &keyPath) const;

    /// Загрузить приватный ключ в память (passphrase кэшируется на сессию)
    ssh_key loadPrivateKey(const QString &keyPath, const QString &passphrase);

    /// Освободить все кэшированные ключи и passphrase (при disconnect)
    void clearCache();

    /// Список ключей из ~/.ssh/
    QList<SshKeyInfo> defaultKeys() const;

signals:
    void passphraseRequired(const QString &keyPath);  ///< UI показывает диалог

private:
    QHash<QString, ssh_key>  m_cache;            ///< path → loaded ssh_key
    QHash<QString, QString>  m_passphraseCache;  ///< path → passphrase (на время сессии)
};

} // namespace linscp::core::keys
