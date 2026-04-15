#pragma once
#include <QObject>
#include <QFile>
#include <QMutex>
#include <QTextStream>

namespace linscp::core::session {

/// Пишет лог SSH-сессии в файл. Аналог WinSCP session log.
/// Один экземпляр на одно подключение; потокобезопасен (QMutex).
///
/// Файл создаётся при start() в директории из AppSettings::sessionLogDir()
/// (по умолчанию ~/.local/share/linscp/logs/).
/// Имя файла: YYYYMMDD_HHmmss_user@host.log
class SessionLogger : public QObject {
    Q_OBJECT
public:
    explicit SessionLogger(QObject *parent = nullptr);
    ~SessionLogger() override;

    /// Открыть новый лог-файл. Безопасно вызывать повторно — закрывает предыдущий.
    void start(const QString &host, const QString &username);

    /// Закрыть файл с финальной строкой.
    void stop();

    /// Путь к текущему открытому файлу (пустой если не запущен).
    QString currentLogFile() const;

public slots:
    /// Записать произвольную строку с timestamp.
    void log(const QString &message);

private:
    mutable QMutex m_mutex;
    QFile          m_file;
    QTextStream    m_stream;
    bool           m_open = false;
};

} // namespace linscp::core::session
