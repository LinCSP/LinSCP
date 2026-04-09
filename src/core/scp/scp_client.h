#pragma once
#include <QString>

namespace linscp::core::scp {

/// SCP fallback для серверов без SFTP.
/// Реализация — v1.x.
class ScpClient {
public:
    // TODO: реализовать через ssh_scp_new / ssh_scp_read / ssh_scp_write
};

} // namespace linscp::core::scp
