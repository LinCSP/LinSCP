#pragma once
#include <QByteArray>
#include <QString>

namespace linscp::utils {

/// Вспомогательные функции шифрования (AES-256-GCM)
/// Используются в SessionStore и других местах где нужно локальное шифрование.
class CryptoUtils {
public:
    struct EncryptedBlob {
        QByteArray iv;          // 12 байт
        QByteArray tag;         // 16 байт
        QByteArray ciphertext;
    };

    /// Зашифровать данные паролем (ключ — SHA-256 от пароля)
    static EncryptedBlob encrypt(const QByteArray &plaintext, const QString &password);

    /// Расшифровать. Возвращает пустой массив при ошибке верификации тега.
    static QByteArray decrypt(const EncryptedBlob &blob, const QString &password);

    /// Удобный вариант: encrypt → сериализация в один QByteArray (IV+TAG+CT)
    static QByteArray encryptToBytes(const QByteArray &data, const QString &password);
    static QByteArray decryptFromBytes(const QByteArray &data, const QString &password);

    /// Сгенерировать криптостойкий случайный пароль длиной n символов
    static QString randomPassword(int n = 32);
};

} // namespace linscp::utils
