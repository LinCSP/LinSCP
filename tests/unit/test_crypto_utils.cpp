#include <QTest>
#include "utils/crypto_utils.h"

using namespace linscp::utils;

class TestCryptoUtils : public QObject {
    Q_OBJECT

private slots:
    void testEncryptDecryptRoundtrip() {
        const QByteArray plain    = "Hello, LinSCP! Секретные данные 12345";
        const QString    password = "test-master-password";

        const QByteArray encrypted = CryptoUtils::encryptToBytes(plain, password);
        QVERIFY(!encrypted.isEmpty());
        QVERIFY(encrypted != plain);

        const QByteArray decrypted = CryptoUtils::decryptFromBytes(encrypted, password);
        QCOMPARE(decrypted, plain);
    }

    void testWrongPasswordFails() {
        const QByteArray plain     = "sensitive data";
        const QByteArray encrypted = CryptoUtils::encryptToBytes(plain, "correct-pass");
        const QByteArray decrypted = CryptoUtils::decryptFromBytes(encrypted, "wrong-pass");
        // AES-GCM должен вернуть пустой массив при неверном теге
        QVERIFY(decrypted.isEmpty());
    }

    void testTruncatedDataFails() {
        const QByteArray encrypted = CryptoUtils::encryptToBytes("data", "pass");
        const QByteArray truncated = encrypted.left(5); // меньше IV+TAG
        const QByteArray result    = CryptoUtils::decryptFromBytes(truncated, "pass");
        QVERIFY(result.isEmpty());
    }

    void testRandomPasswordLength() {
        const QString p16 = CryptoUtils::randomPassword(16);
        const QString p32 = CryptoUtils::randomPassword(32);
        QCOMPARE(p16.size(), 16);
        QCOMPARE(p32.size(), 32);
        // Два вызова дают разные пароли
        QVERIFY(CryptoUtils::randomPassword(32) != CryptoUtils::randomPassword(32));
    }

    void testBlobStructure() {
        const QByteArray data = CryptoUtils::encryptToBytes("test", "pass");
        // Минимальный размер: 12 (IV) + 16 (TAG) + 1 (data) = 29
        QVERIFY(data.size() >= 29);
    }
};

QTEST_MAIN(TestCryptoUtils)
#include "test_crypto_utils.moc"
