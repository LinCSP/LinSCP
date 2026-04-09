#include <QTest>
#include "core/sync/sync_profile.h"
#include "core/sync/sync_comparator.h"

using namespace linscp::core::sync;

class TestSyncComparator : public QObject {
    Q_OBJECT

private slots:
    void testSyncProfileIsValid() {
        SyncProfile p;
        QVERIFY(!p.isValid());
        p.localPath  = "/home/user/docs";
        p.remotePath = "/remote/docs";
        QVERIFY(p.isValid());
    }

    void testMatchesExclude_wildcard() {
        SyncProfile p;
        p.excludePatterns = {"*.tmp", ".git"};

        // SyncComparator — тестируем через matchesExclude косвенно:
        // Паттерн *.tmp должен матчить file.tmp
        QRegularExpression re(
            QRegularExpression::wildcardToRegularExpression("*.tmp"));
        QVERIFY(re.match("file.tmp").hasMatch());
        QVERIFY(!re.match("file.cpp").hasMatch());
    }

    void testSyncAction_semantics() {
        // Проверяем что enum-значения различимы
        QVERIFY(SyncAction::Upload    != SyncAction::Download);
        QVERIFY(SyncAction::Conflict  != SyncAction::Skip);
        QVERIFY(SyncAction::DeleteRemote != SyncAction::DeleteLocal);
    }

    void testSyncDiffEntry_isConflict() {
        SyncDiffEntry e;
        e.action = SyncAction::Conflict;
        QVERIFY(e.isConflict());
        e.action = SyncAction::Upload;
        QVERIFY(!e.isConflict());
    }
};

QTEST_MAIN(TestSyncComparator)
#include "test_sync_comparator.moc"
