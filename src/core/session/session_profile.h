#pragma once
#include <QColor>
#include <QString>
#include <QUuid>
#include "core/ssh/ssh_auth.h"

namespace linscp::core::session {

/// Протокол передачи файлов
enum class TransferProtocol {
    Sftp = 0,   ///< SFTP (предпочтительный)
    Scp  = 1,   ///< SCP (fallback / legacy)
};

/// Тип окончания строк на сервере
enum class EolType {
    LF   = 0,   ///< Unix (по умолчанию)
    CRLF = 1,   ///< Windows
    CR   = 2,   ///< Old Mac
};

/// Режим летнего времени (DST)
enum class DstMode {
    Unix      = 0,  ///< файловая система Unix (без поправки)
    Windows   = 1,  ///< Windows FAT32: вычитать 1ч зимой
    Keep      = 2,  ///< не менять время файлов
};

/// Метод прокси
enum class ProxyMethod {
    None      = 0,
    Socks4    = 1,
    Socks5    = 2,
    Http      = 3,
    Telnet    = 4,
    LocalCmd  = 5,
};

// ─────────────────────────────────────────────────────────────────────────────

/// Настройки SSH-туннеля (jump host)
struct TunnelSettings {
    bool    enabled         = false;
    QString host;
    quint16 port            = 22;
    QString username;
    /// Аутентификация туннельного хоста
    ssh::AuthMethod authMethod  = ssh::AuthMethod::Password;
    QString keyPath;            ///< путь к ключу (если PublicKey)
};

/// Настройки прокси
struct ProxySettings {
    ProxyMethod method      = ProxyMethod::None;
    QString     host;
    quint16     port        = 0;
    QString     username;
    /// Команда (для Telnet/LocalCmd)
    QString     command;
    bool        dnsViaProxy = false;
};

/// Настройки окружения сервера (вкладка Environment в WinSCP)
struct EnvironmentSettings {
    EolType eolType             = EolType::LF;
    DstMode dstMode             = DstMode::Unix;
    bool    utf8FileNames       = true;   ///< Кодировка UTF-8 для имён файлов
    int     timezoneOffsetMin   = 0;      ///< Смещение часового пояса сервера (мин)

    // Корзина
    bool    deleteToRecycleBin  = false;
    QString recycleBinPath;

    // SCP/Shell
    QString shell;                        ///< пустая строка → default shell
    QString listingCommand;               ///< пусто → "ls -la"
    bool    clearAliases        = true;
    bool    ignoreLsWarnings    = true;
    bool    unsetNationalVars   = true;
};

/// Настройки SSH-соединения
struct SshSettings {
    bool compression        = false;
    int  keepaliveInterval  = 0;       ///< 0 — отключён, иначе секунды
    bool tcpNoDelay         = true;
    int  connectTimeout     = 15;      ///< секунды
};

// ─────────────────────────────────────────────────────────────────────────────

/// Полный сохраняемый профиль подключения
struct SessionProfile {
    // Идентификация
    QUuid   id          = QUuid::createUuid();
    QString name;           ///< отображаемое имя
    QString groupPath;      ///< "Production/Web" — иерархия папок

    // Соединение (основной диалог)
    TransferProtocol protocol  = TransferProtocol::Sftp;
    QString  host;
    quint16  port              = 22;
    QString  username;

    // Аутентификация
    ssh::AuthMethod authMethod   = ssh::AuthMethod::Password;
    QString         privateKeyPath;   ///< путь к ключу (если PublicKey)
    QString         keyPassphrase;    ///< passphrase (пусто — спросить при подключении)
    bool            useAgent         = false;

    // Транзиентные поля — не сохраняются на диск, только в памяти сессии
    QString         password;         ///< пароль (не хранится, передаётся при подключении)

    // Пути
    QString initialRemotePath  = "/";
    QString initialLocalPath;
    bool    syncBrowsing       = false;  ///< синхронизировать локальную и удалённую панели

    // Расширенные настройки
    TunnelSettings      tunnel;
    ProxySettings       proxy;
    EnvironmentSettings environment;
    SshSettings         ssh;

    // Метаданные
    QString notes;
    QColor  labelColor;

    bool isValid() const { return !host.isEmpty() && !username.isEmpty(); }
};

} // namespace linscp::core::session
