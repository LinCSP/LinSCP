#include <QTest>
#include "core/transfer/transfer_queue.h"

using namespace linscp::core::transfer;

class TestTransferQueue : public QObject {
    Q_OBJECT

private slots:
    void testEnqueueAndCount() {
        TransferQueue q;
        TransferItem item;
        item.direction  = TransferDirection::Upload;
        item.localPath  = "/tmp/test.txt";
        item.remotePath = "/remote/test.txt";

        const QUuid id = q.enqueue(item);
        QVERIFY(!id.isNull());
        QCOMPARE(q.items().size(), 1);
    }

    void testCancelChangesStatus() {
        TransferQueue q;
        TransferItem item;
        const QUuid id = q.enqueue(item);
        q.cancel(id);
        QCOMPARE(q.item(id).status, TransferStatus::Cancelled);
    }

    void testClearCompleted() {
        TransferQueue q;
        TransferItem item;
        const QUuid id = q.enqueue(item);
        q.setStatus(id, TransferStatus::Completed);
        q.clearCompleted();
        QCOMPARE(q.items().size(), 0);
    }

    void testNextQueued() {
        TransferQueue q;
        TransferItem item;
        item.direction = TransferDirection::Download;
        q.enqueue(item);
        auto next = q.nextQueued();
        QVERIFY(next.has_value());
        QCOMPARE(next->direction, TransferDirection::Download);
    }
};

QTEST_MAIN(TestTransferQueue)
#include "test_transfer_queue.moc"
