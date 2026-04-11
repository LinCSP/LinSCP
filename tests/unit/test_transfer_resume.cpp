#include <QTest>
#include "core/transfer/transfer_item.h"
#include "core/transfer/transfer_queue.h"

using namespace linscp::core::transfer;

class TestTransferResume : public QObject {
    Q_OBJECT

private slots:
    // Структура TransferItem содержит поле resumeOffset для возобновления
    void testResumeOffsetDefault() {
        TransferItem item;
        QCOMPARE(item.resumeOffset, qint64(0));
    }

    void testResumeOffsetSet() {
        TransferItem item;
        item.resumeOffset = 1024 * 1024; // 1 MB
        QCOMPARE(item.resumeOffset, qint64(1024 * 1024));
    }

    // Элемент с resumeOffset попадает в очередь корректно
    void testResumeItemEnqueue() {
        TransferQueue q;
        TransferItem item;
        item.direction    = TransferDirection::Upload;
        item.localPath    = "/tmp/bigfile.bin";
        item.remotePath   = "/remote/bigfile.bin";
        item.totalBytes   = 10 * 1024 * 1024; // 10 MB
        item.resumeOffset = 5 * 1024 * 1024;  // уже передано 5 MB

        const QUuid id = q.enqueue(item);
        QVERIFY(!id.isNull());

        const auto stored = q.item(id);
        QCOMPARE(stored.resumeOffset, qint64(5 * 1024 * 1024));
        QCOMPARE(stored.direction, TransferDirection::Upload);
    }

    // Процент от resumeOffset считается корректно
    void testPercentWithResumeOffset() {
        TransferItem item;
        item.totalBytes       = 100;
        item.transferredBytes = 50;  // половина передана
        QCOMPARE(item.percent(), 50);

        item.transferredBytes = 0;
        QCOMPARE(item.percent(), 0);

        item.transferredBytes = 100;
        QCOMPARE(item.percent(), 100);
    }

    // nextQueued возвращает элемент с resumeOffset
    void testNextQueuedWithResume() {
        TransferQueue q;
        TransferItem item;
        item.direction    = TransferDirection::Download;
        item.resumeOffset = 4096;
        q.enqueue(item);

        const auto next = q.nextQueued();
        QVERIFY(next.has_value());
        QCOMPARE(next->resumeOffset, qint64(4096));
    }
};

QTEST_MAIN(TestTransferResume)
#include "test_transfer_resume.moc"
