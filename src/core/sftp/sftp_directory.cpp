#include "sftp_directory.h"
#include <algorithm>

namespace linscp::core::sftp {

QList<SftpFileInfo> SftpDirectory::files() const
{
    QList<SftpFileInfo> result;
    std::copy_if(entries.begin(), entries.end(), std::back_inserter(result),
                 [](const SftpFileInfo &f) { return !f.isDir; });
    return result;
}

QList<SftpFileInfo> SftpDirectory::dirs() const
{
    QList<SftpFileInfo> result;
    std::copy_if(entries.begin(), entries.end(), std::back_inserter(result),
                 [](const SftpFileInfo &f) { return f.isDir; });
    return result;
}

} // namespace linscp::core::sftp
