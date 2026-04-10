#pragma once
#include <QList>
#include <QString>
#include "session_profile.h"

namespace linscp::core::session {

/// Парсит WinSCP.ini и возвращает список профилей с базовыми настройками.
/// Пароль не импортируется (зашифрован WinSCP).
class WinScpImporter {
public:
    static QList<SessionProfile> import(const QString &iniPath);
};

} // namespace linscp::core::session
