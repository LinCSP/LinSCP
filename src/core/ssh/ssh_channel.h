#pragma once
#include <QObject>
#include <libssh/libssh.h>

namespace linscp::core::ssh {

/// Тип канала поверх одной SSH-сессии
enum class ChannelType {
    Sftp,
    Shell,      ///< для терминала
    Exec,       ///< одиночная команда (remote exec для checksum и т.п.)
};

/// Тонкая обёртка над ssh_channel_t.
/// Канал создаётся через SshSession::openChannel(), не напрямую.
class SshChannel : public QObject {
    Q_OBJECT
public:
    explicit SshChannel(ssh_channel channel, ChannelType type, QObject *parent = nullptr);
    ~SshChannel() override;

    ChannelType type() const { return m_type; }
    ssh_channel handle() const { return m_channel; }
    bool isOpen() const;

    /// Закрыть канал (не сессию)
    void close();

signals:
    void closed();

private:
    ssh_channel m_channel = nullptr;
    ChannelType m_type;
};

} // namespace linscp::core::ssh
