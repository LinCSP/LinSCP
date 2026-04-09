#pragma once
#include <QString>
#include <QDateTime>
#include <QUuid>

namespace linscp::core::transfer {

enum class TransferDirection { Upload, Download };

enum class TransferStatus {
    Queued,
    InProgress,
    Paused,
    Completed,
    Failed,
    Cancelled,
};

/// Один файл в очереди передачи
struct TransferItem {
    QUuid             id          = QUuid::createUuid();
    TransferDirection direction   = TransferDirection::Upload;
    TransferStatus    status      = TransferStatus::Queued;

    QString           localPath;
    QString           remotePath;

    qint64            totalBytes       = 0;
    qint64            transferredBytes = 0;
    qint64            resumeOffset     = 0;  ///< для возобновления

    QDateTime         queuedAt    = QDateTime::currentDateTime();
    QDateTime         startedAt;
    QDateTime         finishedAt;

    QString           errorMessage;

    int percent() const {
        return totalBytes > 0
                   ? static_cast<int>(transferredBytes * 100 / totalBytes)
                   : 0;
    }

    double speedBps() const;   ///< байт/с на основе времени передачи
};

} // namespace linscp::core::transfer
