#include "key_generator.h"
#include <libssh/libssh.h>

namespace linscp::core::keys {

KeyGenerator::KeyGenerator(QObject *parent) : QObject(parent) {}

bool KeyGenerator::generate(GenerateKeyType type, const QString &privateKeyPath,
                            const QString &passphrase, const QString &comment)
{
    Q_UNUSED(comment); // TODO: inject into public key comment field

    ssh_key key = nullptr;

    enum ssh_keytypes_e sshType = SSH_KEYTYPE_ED25519;
    int bits = 0;
    switch (type) {
    case GenerateKeyType::RSA2048:  sshType = SSH_KEYTYPE_RSA; bits = 2048; break;
    case GenerateKeyType::RSA4096:  sshType = SSH_KEYTYPE_RSA; bits = 4096; break;
    case GenerateKeyType::ED25519:  sshType = SSH_KEYTYPE_ED25519; break;
    case GenerateKeyType::ECDSA256: sshType = SSH_KEYTYPE_ECDSA_P256; break;
    }

    if (ssh_pki_generate(sshType, bits, &key) != SSH_OK) {
        m_lastError = tr("Key generation failed");
        emit errorOccurred(m_lastError);
        return false;
    }

    const char *pass = passphrase.isEmpty() ? nullptr : passphrase.toUtf8().constData();
    if (ssh_pki_export_privkey_file(key, pass, nullptr, nullptr,
                                    privateKeyPath.toUtf8().constData()) != SSH_OK) {
        ssh_key_free(key);
        m_lastError = tr("Cannot write private key to: %1").arg(privateKeyPath);
        emit errorOccurred(m_lastError);
        return false;
    }

    const QString pubPath = privateKeyPath + ".pub";
    if (ssh_pki_export_pubkey_file(key, pubPath.toUtf8().constData()) != SSH_OK) {
        ssh_key_free(key);
        m_lastError = tr("Cannot write public key to: %1").arg(pubPath);
        emit errorOccurred(m_lastError);
        return false;
    }

    ssh_key_free(key);
    emit generated(privateKeyPath, pubPath);
    return true;
}

} // namespace linscp::core::keys
