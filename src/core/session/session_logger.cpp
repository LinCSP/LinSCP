#include "session_logger.h"
#include "core/app_settings.h"

#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

namespace linscp::core::session {

SessionLogger::SessionLogger(QObject *parent)
    : QObject(parent)
{}

SessionLogger::~SessionLogger()
{
    stop();
}

void SessionLogger::start(const QString &host, const QString &username)
{
    QMutexLocker lock(&m_mutex);
    if (m_open) {
        m_stream << QStringLiteral("[%1] Session closed (restarting).\n")
                        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
        m_stream.flush();
        m_file.close();
        m_open = false;
    }

    QString dir = core::AppSettings::sessionLogDir();
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
              + QStringLiteral("/logs");
    QDir().mkpath(dir);

    const QString ts   = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString name = QStringLiteral("%1_%2@%3.log").arg(ts, username, host);
    m_file.setFileName(dir + QLatin1Char('/') + name);

    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    m_stream.setDevice(&m_file);
    m_open = true;

    m_stream << QStringLiteral("=== LinSCP Session Log ===\n")
             << QStringLiteral("Started : %1\n")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
             << QStringLiteral("Host    : %1\n").arg(host)
             << QStringLiteral("User    : %1\n").arg(username)
             << QStringLiteral("=========================\n");
    m_stream.flush();
}

void SessionLogger::stop()
{
    QMutexLocker lock(&m_mutex);
    if (!m_open) return;

    m_stream << QStringLiteral("[%1] Session ended.\n")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    m_stream.flush();
    m_file.close();
    m_open = false;
}

QString SessionLogger::currentLogFile() const
{
    QMutexLocker lock(&m_mutex);
    return m_open ? m_file.fileName() : QString{};
}

void SessionLogger::log(const QString &message)
{
    QMutexLocker lock(&m_mutex);
    if (!m_open) return;

    m_stream << QStringLiteral("[%1] %2\n")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                         message);
    m_stream.flush();
}

} // namespace linscp::core::session
