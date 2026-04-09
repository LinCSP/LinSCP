#include "session_store.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace linscp::core::session {

static constexpr int kKeyLen  = 32; // AES-256
static constexpr int kIvLen   = 12; // GCM
static constexpr int kTagLen  = 16;

SessionStore::SessionStore(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(filePath) {}

// ── JSON сериализация ─────────────────────────────────────────────────────────

static QJsonObject profileToJson(const SessionProfile &p)
{
    QJsonObject o;
    o["id"]              = p.id.toString();
    o["name"]            = p.name;
    o["groupPath"]       = p.groupPath;
    o["host"]            = p.host;
    o["port"]            = p.port;
    o["username"]        = p.username;
    o["authMethod"]      = static_cast<int>(p.authMethod);
    o["privateKeyPath"]  = p.privateKeyPath;
    o["useAgent"]        = p.useAgent;
    o["initialRemote"]   = p.initialRemotePath;
    o["initialLocal"]    = p.initialLocalPath;
    o["notes"]           = p.notes;
    return o;
}

static SessionProfile jsonToProfile(const QJsonObject &o)
{
    SessionProfile p;
    p.id              = QUuid::fromString(o["id"].toString());
    p.name            = o["name"].toString();
    p.groupPath       = o["groupPath"].toString();
    p.host            = o["host"].toString();
    p.port            = static_cast<quint16>(o["port"].toInt(22));
    p.username        = o["username"].toString();
    p.authMethod      = static_cast<ssh::AuthMethod>(o["authMethod"].toInt());
    p.privateKeyPath  = o["privateKeyPath"].toString();
    p.useAgent        = o["useAgent"].toBool();
    p.initialRemotePath = o["initialRemote"].toString("/");
    p.initialLocalPath  = o["initialLocal"].toString();
    p.notes           = o["notes"].toString();
    return p;
}

// ── Crypto (AES-256-GCM) ──────────────────────────────────────────────────────

QByteArray SessionStore::encrypt(const QByteArray &data, const QString &password) const
{
    // Derive key from password with SHA-256 (TODO: upgrade to Argon2)
    const QByteArray key = QCryptographicHash::hash(
        password.toUtf8(), QCryptographicHash::Sha256);

    QByteArray iv(kIvLen, 0);
    RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()), kIvLen);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                       reinterpret_cast<const unsigned char *>(key.constData()),
                       reinterpret_cast<const unsigned char *>(iv.constData()));

    QByteArray cipher(data.size() + kTagLen, 0);
    int outLen = 0;
    EVP_EncryptUpdate(ctx,
                      reinterpret_cast<unsigned char *>(cipher.data()), &outLen,
                      reinterpret_cast<const unsigned char *>(data.constData()),
                      data.size());

    int finalLen = 0;
    EVP_EncryptFinal_ex(ctx,
                        reinterpret_cast<unsigned char *>(cipher.data()) + outLen,
                        &finalLen);

    QByteArray tag(kTagLen, 0);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen,
                        reinterpret_cast<unsigned char *>(tag.data()));
    EVP_CIPHER_CTX_free(ctx);

    // Формат: IV(12) + TAG(16) + CIPHERTEXT
    return iv + tag + cipher.left(outLen + finalLen);
}

QByteArray SessionStore::decrypt(const QByteArray &data, const QString &password) const
{
    if (data.size() < kIvLen + kTagLen) return {};

    const QByteArray key = QCryptographicHash::hash(
        password.toUtf8(), QCryptographicHash::Sha256);

    const QByteArray iv     = data.left(kIvLen);
    const QByteArray tag    = data.mid(kIvLen, kTagLen);
    const QByteArray cipher = data.mid(kIvLen + kTagLen);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                       reinterpret_cast<const unsigned char *>(key.constData()),
                       reinterpret_cast<const unsigned char *>(iv.constData()));
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen,
                        const_cast<char *>(tag.constData()));

    QByteArray plain(cipher.size(), 0);
    int outLen = 0;
    EVP_DecryptUpdate(ctx,
                      reinterpret_cast<unsigned char *>(plain.data()), &outLen,
                      reinterpret_cast<const unsigned char *>(cipher.constData()),
                      cipher.size());

    int finalLen = 0;
    const bool ok = EVP_DecryptFinal_ex(
        ctx, reinterpret_cast<unsigned char *>(plain.data()) + outLen, &finalLen) > 0;
    EVP_CIPHER_CTX_free(ctx);

    return ok ? plain.left(outLen + finalLen) : QByteArray{};
}

// ── Public API ────────────────────────────────────────────────────────────────

bool SessionStore::load(const QString &masterPassword)
{
    QFile f(m_filePath);
    if (!f.open(QIODevice::ReadOnly)) return true; // первый запуск

    QByteArray raw = f.readAll();
    if (!masterPassword.isEmpty()) raw = decrypt(raw, masterPassword);

    const QJsonArray arr = QJsonDocument::fromJson(raw).array();
    m_profiles.clear();
    for (const auto &v : arr)
        m_profiles.append(jsonToProfile(v.toObject()));
    return true;
}

bool SessionStore::save(const QString &masterPassword) const
{
    QJsonArray arr;
    for (const auto &p : m_profiles) arr.append(profileToJson(p));
    QByteArray data = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    if (!masterPassword.isEmpty()) data = encrypt(data, masterPassword);

    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(data);
    return true;
}

QList<SessionProfile> SessionStore::all() const { return m_profiles; }

SessionProfile SessionStore::find(const QUuid &id) const
{
    for (const auto &p : m_profiles) if (p.id == id) return p;
    return {};
}

void SessionStore::add(const SessionProfile &profile)
{
    m_profiles.append(profile);
    emit changed();
}

void SessionStore::update(const SessionProfile &profile)
{
    for (auto &p : m_profiles) {
        if (p.id == profile.id) { p = profile; emit changed(); return; }
    }
}

void SessionStore::remove(const QUuid &id)
{
    m_profiles.removeIf([&id](const SessionProfile &p) { return p.id == id; });
    emit changed();
}

bool SessionStore::exportJson(const QString &path) const
{
    QJsonArray arr;
    for (const auto &p : m_profiles) arr.append(profileToJson(p));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(arr).toJson());
    return true;
}

bool SessionStore::importJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const auto &v : arr) m_profiles.append(jsonToProfile(v.toObject()));
    emit changed();
    return true;
}

} // namespace linscp::core::session
