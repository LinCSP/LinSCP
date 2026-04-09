#include "ssh_channel.h"
#include <libssh/libssh.h>

namespace linscp::core::ssh {

SshChannel::SshChannel(ssh_channel channel, ChannelType type, QObject *parent)
    : QObject(parent), m_channel(channel), m_type(type)
{}

SshChannel::~SshChannel()
{
    close();
    if (m_channel) {
        ssh_channel_free(m_channel);
        m_channel = nullptr;
    }
}

bool SshChannel::isOpen() const
{
    return m_channel && ssh_channel_is_open(m_channel);
}

void SshChannel::close()
{
    if (isOpen()) {
        ssh_channel_send_eof(m_channel);
        ssh_channel_close(m_channel);
        emit closed();
    }
}

} // namespace linscp::core::ssh
