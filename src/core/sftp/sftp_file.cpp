#include "sftp_file.h"

namespace linscp::core::sftp {

QString SftpFileInfo::permissionsString() const
{
    QString s;
    s += isDir ? 'd' : (isSymLink ? 'l' : '-');
    s += (permissions & 0400) ? 'r' : '-';
    s += (permissions & 0200) ? 'w' : '-';
    s += (permissions & 0100) ? 'x' : '-';
    s += (permissions & 0040) ? 'r' : '-';
    s += (permissions & 0020) ? 'w' : '-';
    s += (permissions & 0010) ? 'x' : '-';
    s += (permissions & 0004) ? 'r' : '-';
    s += (permissions & 0002) ? 'w' : '-';
    s += (permissions & 0001) ? 'x' : '-';
    return s;
}

} // namespace linscp::core::sftp
