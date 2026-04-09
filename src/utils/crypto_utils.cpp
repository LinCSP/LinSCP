#include "crypto_utils.h"
#include <QCryptographicHash>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace linscp::utils {

static constexpr int kKeyLen = 32;
static constexpr int kIvLen  = 12;
static constexpr int kTagLen = 16;

static QByteArray deriveKey(const QString &password)
{
    return QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
}

CryptoUtils::EncryptedBlob CryptoUtils::encrypt(const QByteArray &plaintext,
                                                 const QString &password)
{
    EncryptedBlob blob;
    blob.iv.resize(kIvLen);
    RAND_bytes(reinterpret_cast<unsigned char *>(blob.iv.data()), kIvLen);

    const QByteArray key = deriveKey(password);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                       reinterpret_cast<const unsigned char *>(key.constData()),
                       reinterpret_cast<const unsigned char *>(blob.iv.constData()));

    blob.ciphertext.resize(plaintext.size());
    int outLen = 0;
    EVP_EncryptUpdate(ctx,
                      reinterpret_cast<unsigned char *>(blob.ciphertext.data()), &outLen,
                      reinterpret_cast<const unsigned char *>(plaintext.constData()),
                      plaintext.size());

    int finalLen = 0;
    EVP_EncryptFinal_ex(ctx,
                        reinterpret_cast<unsigned char *>(blob.ciphertext.data()) + outLen,
                        &finalLen);
    blob.ciphertext.resize(outLen + finalLen);

    blob.tag.resize(kTagLen);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen,
                        reinterpret_cast<unsigned char *>(blob.tag.data()));
    EVP_CIPHER_CTX_free(ctx);
    return blob;
}

QByteArray CryptoUtils::decrypt(const EncryptedBlob &blob, const QString &password)
{
    const QByteArray key = deriveKey(password);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                       reinterpret_cast<const unsigned char *>(key.constData()),
                       reinterpret_cast<const unsigned char *>(blob.iv.constData()));
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen,
                        const_cast<char *>(blob.tag.constData()));

    QByteArray plain(blob.ciphertext.size(), 0);
    int outLen = 0;
    EVP_DecryptUpdate(ctx,
                      reinterpret_cast<unsigned char *>(plain.data()), &outLen,
                      reinterpret_cast<const unsigned char *>(blob.ciphertext.constData()),
                      blob.ciphertext.size());

    int finalLen = 0;
    const bool ok = EVP_DecryptFinal_ex(
        ctx, reinterpret_cast<unsigned char *>(plain.data()) + outLen, &finalLen) > 0;
    EVP_CIPHER_CTX_free(ctx);

    return ok ? plain.left(outLen + finalLen) : QByteArray{};
}

QByteArray CryptoUtils::encryptToBytes(const QByteArray &data, const QString &password)
{
    const auto blob = encrypt(data, password);
    return blob.iv + blob.tag + blob.ciphertext;
}

QByteArray CryptoUtils::decryptFromBytes(const QByteArray &data, const QString &password)
{
    if (data.size() < kIvLen + kTagLen) return {};
    EncryptedBlob blob;
    blob.iv         = data.left(kIvLen);
    blob.tag        = data.mid(kIvLen, kTagLen);
    blob.ciphertext = data.mid(kIvLen + kTagLen);
    return decrypt(blob, password);
}

QString CryptoUtils::randomPassword(int n)
{
    const QByteArray charset =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*";
    QByteArray buf(n, 0);
    RAND_bytes(reinterpret_cast<unsigned char *>(buf.data()), n);
    QString result;
    result.reserve(n);
    for (unsigned char c : buf)
        result += charset[c % charset.size()];
    return result;
}

} // namespace linscp::utils
