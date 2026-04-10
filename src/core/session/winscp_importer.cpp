#include "winscp_importer.h"
#include <QFile>
#include <QHash>
#include <QTextStream>
#include <QUrl>
#include <QRegularExpression>

namespace linscp::core::session {

// Конвертация Windows-пути из WinSCP (URL-кодированного) в Linux-путь.
// "Z:%5Chome%5Calex%5Cdevel"  →  "/home/alex/devel"
// "%EF%BB%BFZ:%5C..."         →  "/..."   (BOM убирается)
static QString convertWinPath(const QString &raw)
{
    QString s = QUrl::fromPercentEncoding(raw.toUtf8());
    s.remove(QChar(0xFEFF));          // убрать BOM
    s = s.trimmed();
    if (s.isEmpty()) return {};

    // Убрать букву диска "X:" или "X:/"
    static const QRegularExpression driveRe(QStringLiteral("^[A-Za-z]:"));
    s.remove(driveRe);

    s.replace('\\', '/');
    if (!s.startsWith('/')) s.prepend('/');
    return s;
}

// Из имени секции вида "Sessions\Moy/A-media/Cobalt-pro/cob-alt.ru"
// получить URL-декодированный путь "Moy/A-media/Cobalt-pro/cob-alt.ru"
static QString decodeSessionSection(const QString &sectionKey)
{
    const int bs = sectionKey.indexOf('\\');
    QString path = (bs >= 0) ? sectionKey.mid(bs + 1) : sectionKey;
    path = QUrl::fromPercentEncoding(path.toUtf8());
    path.remove(QChar(0xFEFF));
    return path.trimmed();
}

QList<SessionProfile> WinScpImporter::import(const QString &iniPath)
{
    QFile f(iniPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);

    QList<SessionProfile>  result;
    QString                currentSection;
    QHash<QString, QString> data;

    auto flushSection = [&]() {
        if (!currentSection.startsWith(QLatin1String("Sessions\\"),
                                       Qt::CaseInsensitive))
            return;

        const QString fullPath = decodeSessionSection(currentSection);
        if (fullPath.isEmpty()) return;

        const QString hostName = data.value(QStringLiteral("HostName"));
        if (hostName.isEmpty()) return;

        const int lastSlash = fullPath.lastIndexOf('/');
        SessionProfile p;
        p.name      = (lastSlash >= 0) ? fullPath.mid(lastSlash + 1) : fullPath;
        p.groupPath = (lastSlash >= 0) ? fullPath.left(lastSlash)    : QString{};
        p.host      = hostName;
        p.port      = static_cast<quint16>(
                          data.value(QStringLiteral("PortNumber"), QStringLiteral("22")).toUInt());
        p.username  = data.value(QStringLiteral("UserName"));

        // FSProtocol: 0=SCP, 1=SFTP+SCP, 2=SFTP
        const int fsProt = data.value(QStringLiteral("FSProtocol"), QStringLiteral("2")).toInt();
        p.protocol = (fsProt == 0) ? TransferProtocol::Scp : TransferProtocol::Sftp;

        // Аутентификация
        const QString pubKey = data.value(QStringLiteral("PublicKeyFile"));
        if (!pubKey.isEmpty()) {
            p.authMethod     = ssh::AuthMethod::PublicKey;
            p.privateKeyPath = convertWinPath(pubKey);
        } else if (data.value(QStringLiteral("TryAgent"), QStringLiteral("1")).toInt() == 1) {
            p.authMethod = ssh::AuthMethod::PublicKey;
            p.useAgent   = true;
        }
        // Password — зашифрован WinSCP, не импортируем

        // Начальные пути
        const QString remDir = data.value(QStringLiteral("RemoteDirectory"));
        if (!remDir.isEmpty()) p.initialRemotePath = remDir;

        const QString locDir = data.value(QStringLiteral("LocalDirectory"));
        if (!locDir.isEmpty()) p.initialLocalPath = convertWinPath(locDir);

        result.append(p);
    };

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('#'))
            continue;

        if (line.startsWith('[') && line.endsWith(']')) {
            flushSection();
            currentSection = line.mid(1, line.size() - 2);
            data.clear();
        } else {
            const int eq = line.indexOf('=');
            if (eq > 0)
                data.insert(line.left(eq), line.mid(eq + 1));
        }
    }
    flushSection();

    return result;
}

} // namespace linscp::core::session
