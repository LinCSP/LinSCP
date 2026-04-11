#include "key_manager.h"
#include <QDir>
#include <QDirIterator>
#include <libssh/libssh.h>

namespace linscp::core::keys {

KeyManager::KeyManager(QObject *parent) : QObject(parent) {}

KeyManager::~KeyManager()
{
    clearCache();
}

void KeyManager::clearCache()
{
    for (auto &k : m_cache) ssh_key_free(k);
    m_cache.clear();
    m_passphraseCache.clear();
}

SshKeyInfo KeyManager::inspect(const QString &keyPath) const
{
    SshKeyInfo info;
    info.path = keyPath;

    ssh_key key = nullptr;
    // Пробуем без пароля; если провалится — значит защищён
    int rc = ssh_pki_import_privkey_file(keyPath.toUtf8().constData(),
                                         nullptr, nullptr, nullptr, &key);
    if (rc == SSH_ERROR) {
        // Скорее всего есть passphrase — проверяем публичный ключ
        info.hasPassphrase = true;
        ssh_key pub = nullptr;
        QString pubPath = keyPath + ".pub";
        if (ssh_pki_import_pubkey_file(pubPath.toUtf8().constData(), &pub) == SSH_OK) {
            unsigned char *hash = nullptr; size_t len = 0;
            ssh_get_publickey_hash(pub, SSH_PUBLICKEY_HASH_SHA256, &hash, &len);
            info.fingerprint = "SHA256:" + QString::fromUtf8(
                QByteArray(reinterpret_cast<char*>(hash), static_cast<int>(len)).toBase64());
            ssh_clean_pubkey_hash(&hash);
            ssh_key_free(pub);
        }
        return info;
    }

    if (key) {
        unsigned char *hash = nullptr; size_t len = 0;
        ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &len);
        info.fingerprint = "SHA256:" + QString::fromUtf8(
            QByteArray(reinterpret_cast<char*>(hash), static_cast<int>(len)).toBase64());
        ssh_clean_pubkey_hash(&hash);

        switch (ssh_key_type(key)) {
        case SSH_KEYTYPE_RSA:     info.type = KeyType::RSA;     break;
        case SSH_KEYTYPE_ED25519: info.type = KeyType::ED25519; break;
        case SSH_KEYTYPE_ECDSA_P256:
        case SSH_KEYTYPE_ECDSA_P384:
        case SSH_KEYTYPE_ECDSA_P521:
            info.type = KeyType::ECDSA; break;
        default: break;
        }
        ssh_key_free(key);
    }
    return info;
}

ssh_key KeyManager::loadPrivateKey(const QString &keyPath, const QString &passphrase)
{
    if (m_cache.contains(keyPath)) return m_cache[keyPath];

    // Используем сохранённый passphrase, если вызывающий не передал новый
    const QString effectivePass = passphrase.isEmpty()
                                  ? m_passphraseCache.value(keyPath)
                                  : passphrase;

    ssh_key key = nullptr;
    const char *pass = effectivePass.isEmpty()
                       ? nullptr
                       : effectivePass.toUtf8().constData();
    if (ssh_pki_import_privkey_file(keyPath.toUtf8().constData(),
                                    pass, nullptr, nullptr, &key) != SSH_OK)
        return nullptr;

    m_cache[keyPath] = key;
    if (!effectivePass.isEmpty())
        m_passphraseCache[keyPath] = effectivePass;  // кэшируем passphrase на сессию
    return key;
}

QList<SshKeyInfo> KeyManager::defaultKeys() const
{
    QList<SshKeyInfo> result;
    const QString sshDir = QDir::homePath() + "/.ssh";
    QDirIterator it(sshDir, {"id_rsa", "id_ed25519", "id_ecdsa", "*.pem"},
                    QDir::Files);
    while (it.hasNext()) {
        it.next();
        result.append(inspect(it.filePath()));
    }
    return result;
}

} // namespace linscp::core::keys
