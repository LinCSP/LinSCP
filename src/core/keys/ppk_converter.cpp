#include "ppk_converter.h"
#include <libssh/libssh.h>
#include <QFile>

namespace linscp::core::keys {

thread_local QString PpkConverter::s_lastError;

QString PpkConverter::lastError() { return s_lastError; }

// libssh >= 0.10 поддерживает импорт .ppk напрямую через ssh_pki_import_privkey_file.
// Для более ранних версий необходима своя реализация парсера .ppk.
bool PpkConverter::ppkToOpenSsh(const QString &ppkPath,
                                 const QString &outPath,
                                 const QString &passphrase)
{
    ssh_key key = nullptr;
    const char *pass = passphrase.isEmpty() ? nullptr : passphrase.toUtf8().constData();

    if (ssh_pki_import_privkey_file(ppkPath.toUtf8().constData(),
                                    pass, nullptr, nullptr, &key) != SSH_OK) {
        s_lastError = QStringLiteral("Cannot import .ppk: %1").arg(ppkPath);
        return false;
    }

    if (ssh_pki_export_privkey_file(key, nullptr, nullptr, nullptr,
                                    outPath.toUtf8().constData()) != SSH_OK) {
        ssh_key_free(key);
        s_lastError = QStringLiteral("Cannot export OpenSSH key to: %1").arg(outPath);
        return false;
    }

    ssh_key_free(key);
    return true;
}

bool PpkConverter::openSshToPpk(const QString &keyPath, const QString &outPath,
                                 const QString &passphrase, const QString &ppkComment)
{
    Q_UNUSED(ppkComment); // TODO: embed into .ppk Comment field manually

    ssh_key key = nullptr;
    const char *pass = passphrase.isEmpty() ? nullptr : passphrase.toUtf8().constData();

    if (ssh_pki_import_privkey_file(keyPath.toUtf8().constData(),
                                    pass, nullptr, nullptr, &key) != SSH_OK) {
        s_lastError = QStringLiteral("Cannot load key: %1").arg(keyPath);
        return false;
    }

    // libssh не умеет экспортировать в .ppk — нужна ручная сериализация.
    // TODO: реализовать ppk v3 writer (см. putty/sshpubk.c в репозитории PuTTY)
    ssh_key_free(key);
    s_lastError = QStringLiteral("OpenSSH→PPK export not yet implemented");
    Q_UNUSED(outPath);
    return false;
}

} // namespace linscp::core::keys
