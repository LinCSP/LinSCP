#pragma once
#include <QString>

namespace linscp::core::keys {

/// Конвертация PuTTY .ppk (v2/v3) ↔ OpenSSH
/// Логика аналогична WinSCP/PuTTYgen (см. putty/import.c)
class PpkConverter {
public:
    /// .ppk → OpenSSH private key файл
    static bool ppkToOpenSsh(const QString &ppkPath,
                              const QString &outPath,
                              const QString &passphrase = {});

    /// OpenSSH → .ppk v3
    static bool openSshToPpk(const QString &keyPath,
                              const QString &outPath,
                              const QString &passphrase   = {},
                              const QString &ppkComment   = {});

    static QString lastError();

private:
    static thread_local QString s_lastError;
};

} // namespace linscp::core::keys
