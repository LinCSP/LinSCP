#include "transfer_item.h"

namespace linscp::core::transfer {

double TransferItem::speedBps() const
{
    if (!startedAt.isValid()) return 0.0;
    const QDateTime end = finishedAt.isValid() ? finishedAt : QDateTime::currentDateTime();
    const double secs = startedAt.msecsTo(end) / 1000.0;
    return secs > 0 ? static_cast<double>(transferredBytes - resumeOffset) / secs : 0.0;
}

} // namespace linscp::core::transfer
