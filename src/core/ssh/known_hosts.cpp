#include "known_hosts.h"
#include <QFile>
#include <QTextStream>
#include <QDir>

namespace linscp::core::ssh {

KnownHosts::KnownHosts(const QString &filePath)
    : m_filePath(filePath)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
}

HostVerifyResult KnownHosts::verify(const QString &host, int port,
                                    const QByteArray &fingerprint) const
{
    QFile f(m_filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return HostVerifyResult::NewHost;

    const QString key = QStringLiteral("%1:%2").arg(host).arg(port);
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith('#') || line.isEmpty()) continue;

        const auto parts = line.split(' ');
        if (parts.size() < 2) continue;
        if (parts[0] != key) continue;

        const QByteArray stored = QByteArray::fromBase64(parts[1].toUtf8());
        return (stored == fingerprint) ? HostVerifyResult::Ok
                                       : HostVerifyResult::Changed;
    }
    return HostVerifyResult::NewHost;
}

bool KnownHosts::accept(const QString &host, int port, const QByteArray &fingerprint)
{
    remove(host, port); // убрать старую запись если была

    QFile f(m_filePath);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return false;

    QTextStream out(&f);
    out << QStringLiteral("%1:%2 %3\n")
               .arg(host).arg(port)
               .arg(QString::fromUtf8(fingerprint.toBase64()));
    return true;
}

bool KnownHosts::remove(const QString &host, int port)
{
    QFile f(m_filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    const QString key = QStringLiteral("%1:%2").arg(host).arg(port);
    QStringList lines;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!line.startsWith(key))
            lines << line;
    }
    f.close();

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream out(&f);
    for (const auto &l : lines) out << l << '\n';
    return true;
}

} // namespace linscp::core::ssh
