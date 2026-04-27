#include "session_store.h"
#include "core/webdav/webdav_client.h"
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
    // Базовые
    o["id"]              = p.id.toString();
    o["name"]            = p.name;
    o["groupPath"]       = p.groupPath;
    o["protocol"]        = static_cast<int>(p.protocol);
    o["host"]            = p.host;
    o["port"]            = p.port;
    o["username"]        = p.username;
    o["authMethod"]      = static_cast<int>(p.authMethod);
    o["privateKeyPath"]  = p.privateKeyPath;
    o["useAgent"]        = p.useAgent;
    o["password"]        = p.password;
    o["initialRemote"]   = p.initialRemotePath;
    o["initialLocal"]    = p.initialLocalPath;
    o["syncBrowsing"]    = p.syncBrowsing;
    o["notes"]           = p.notes;

    // Среда
    QJsonObject env;
    env["eolType"]            = static_cast<int>(p.environment.eolType);
    env["dstMode"]            = static_cast<int>(p.environment.dstMode);
    env["utf8FileNames"]      = p.environment.utf8FileNames;
    env["timezoneOffsetMin"]  = p.environment.timezoneOffsetMin;
    env["deleteToRecycleBin"] = p.environment.deleteToRecycleBin;
    env["recycleBinPath"]     = p.environment.recycleBinPath;
    env["shell"]              = p.environment.shell;
    env["listingCommand"]     = p.environment.listingCommand;
    env["clearAliases"]       = p.environment.clearAliases;
    env["ignoreLsWarnings"]   = p.environment.ignoreLsWarnings;
    env["unsetNationalVars"]  = p.environment.unsetNationalVars;
    o["environment"] = env;

    // Прокси
    QJsonObject prx;
    prx["method"]      = static_cast<int>(p.proxy.method);
    prx["host"]        = p.proxy.host;
    prx["port"]        = p.proxy.port;
    prx["username"]    = p.proxy.username;
    prx["command"]     = p.proxy.command;
    prx["dnsViaProxy"] = p.proxy.dnsViaProxy;
    o["proxy"] = prx;

    // Туннель
    QJsonObject tun;
    tun["enabled"]    = p.tunnel.enabled;
    tun["host"]       = p.tunnel.host;
    tun["port"]       = p.tunnel.port;
    tun["username"]   = p.tunnel.username;
    tun["authMethod"] = static_cast<int>(p.tunnel.authMethod);
    tun["keyPath"]    = p.tunnel.keyPath;
    o["tunnel"] = tun;

    // SSH
    QJsonObject ssh;
    ssh["compression"]       = p.ssh.compression;
    ssh["keepaliveInterval"] = p.ssh.keepaliveInterval;
    ssh["tcpNoDelay"]        = p.ssh.tcpNoDelay;
    ssh["connectTimeout"]    = p.ssh.connectTimeout;
    o["ssh"] = ssh;

    // WebDAV
    QJsonObject dav;
    dav["encryption"] = static_cast<int>(p.webDavEncryption);
    dav["path"]       = p.webDavPath;
    o["webdav"] = dav;

    return o;
}

static SessionProfile jsonToProfile(const QJsonObject &o)
{
    SessionProfile p;
    // Базовые
    p.id              = QUuid::fromString(o["id"].toString());
    p.name            = o["name"].toString();
    p.groupPath       = o["groupPath"].toString();
    p.protocol        = static_cast<TransferProtocol>(o["protocol"].toInt(0));
    p.host            = o["host"].toString();
    p.port            = static_cast<quint16>(o["port"].toInt(22));
    p.username        = o["username"].toString();
    p.authMethod      = static_cast<ssh::AuthMethod>(o["authMethod"].toInt());
    p.privateKeyPath  = o["privateKeyPath"].toString();
    p.useAgent        = o["useAgent"].toBool();
    p.password        = o["password"].toString();
    p.initialRemotePath = o["initialRemote"].toString("/");
    p.initialLocalPath  = o["initialLocal"].toString();
    p.syncBrowsing    = o["syncBrowsing"].toBool();
    p.notes           = o["notes"].toString();

    // Среда
    const QJsonObject env = o["environment"].toObject();
    p.environment.eolType            = static_cast<EolType>(env["eolType"].toInt(0));
    p.environment.dstMode            = static_cast<DstMode>(env["dstMode"].toInt(0));
    p.environment.utf8FileNames      = env["utf8FileNames"].toBool(true);
    p.environment.timezoneOffsetMin  = env["timezoneOffsetMin"].toInt(0);
    p.environment.deleteToRecycleBin = env["deleteToRecycleBin"].toBool();
    p.environment.recycleBinPath     = env["recycleBinPath"].toString();
    p.environment.shell              = env["shell"].toString();
    p.environment.listingCommand     = env["listingCommand"].toString();
    p.environment.clearAliases       = env["clearAliases"].toBool(true);
    p.environment.ignoreLsWarnings   = env["ignoreLsWarnings"].toBool(true);
    p.environment.unsetNationalVars  = env["unsetNationalVars"].toBool(true);

    // Прокси
    const QJsonObject prx = o["proxy"].toObject();
    p.proxy.method      = static_cast<ProxyMethod>(prx["method"].toInt(0));
    p.proxy.host        = prx["host"].toString();
    p.proxy.port        = static_cast<quint16>(prx["port"].toInt(0));
    p.proxy.username    = prx["username"].toString();
    p.proxy.command     = prx["command"].toString();
    p.proxy.dnsViaProxy = prx["dnsViaProxy"].toBool();

    // Туннель
    const QJsonObject tun = o["tunnel"].toObject();
    p.tunnel.enabled    = tun["enabled"].toBool();
    p.tunnel.host       = tun["host"].toString();
    p.tunnel.port       = static_cast<quint16>(tun["port"].toInt(22));
    p.tunnel.username   = tun["username"].toString();
    p.tunnel.authMethod = static_cast<ssh::AuthMethod>(tun["authMethod"].toInt(0));
    p.tunnel.keyPath    = tun["keyPath"].toString();

    // SSH
    const QJsonObject ssh = o["ssh"].toObject();
    p.ssh.compression       = ssh["compression"].toBool();
    p.ssh.keepaliveInterval = ssh["keepaliveInterval"].toInt(0);
    p.ssh.tcpNoDelay        = ssh["tcpNoDelay"].toBool(true);
    p.ssh.connectTimeout    = ssh["connectTimeout"].toInt(15);

    // WebDAV
    const QJsonObject dav = o["webdav"].toObject();
    p.webDavEncryption = static_cast<webdav::WebDavEncryption>(dav["encryption"].toInt(0));
    p.webDavPath       = dav["path"].toString("/");

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

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    // Формат: { "sessions": [...], "folders": [...] }
    // Для обратной совместимости поддерживаем и старый формат — голый массив
    QJsonArray arr;
    m_folders.clear();
    if (doc.isObject()) {
        arr = doc.object()["sessions"].toArray();
        for (const auto &v : doc.object()["folders"].toArray())
            m_folders.append(v.toString());
    } else {
        arr = doc.array();
    }
    m_profiles.clear();
    for (const auto &v : arr)
        m_profiles.append(jsonToProfile(v.toObject()));
    return true;
}

bool SessionStore::save(const QString &masterPassword) const
{
    QJsonArray arr;
    for (const auto &p : m_profiles) arr.append(profileToJson(p));
    QJsonArray foldersArr;
    for (const auto &f : m_folders) foldersArr.append(f);
    QJsonObject root;
    root["sessions"] = arr;
    root["folders"]  = foldersArr;
    QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
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

QStringList SessionStore::folders() const { return m_folders; }

void SessionStore::addFolder(const QString &path)
{
    if (!m_folders.contains(path)) {
        m_folders.append(path);
        emit changed();
    }
}

void SessionStore::removeFolder(const QString &path)
{
    // Удаляем папку и все её дочерние пути
    m_folders.removeIf([&path](const QString &f) {
        return f == path || f.startsWith(path + '/');
    });
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
