#pragma once
#include <QColor>
#include <QString>
#include <QUuid>
#include "core/ssh/ssh_auth.h"

namespace linscp::core::session {

/// Сохраняемый профиль подключения
struct SessionProfile {
    QUuid   id       = QUuid::createUuid();
    QString name;           ///< отображаемое имя
    QString groupPath;      ///< "Production/Web" — иерархия папок

    QString host;
    quint16 port      = 22;
    QString username;

    ssh::AuthMethod authMethod   = ssh::AuthMethod::Password;
    QString         privateKeyPath;  ///< если PublicKey
    bool            useAgent     = false;

    QString         initialRemotePath = "/";
    QString         initialLocalPath;

    QString         notes;
    QColor          labelColor;     ///< цвет метки в списке сессий

    bool isValid() const { return !host.isEmpty() && !username.isEmpty(); }
};

} // namespace linscp::core::session
