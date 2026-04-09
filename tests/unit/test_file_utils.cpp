#include <QTest>
#include "utils/file_utils.h"

using namespace linscp::utils;

class TestFileUtils : public QObject {
    Q_OBJECT

private slots:
    void testHumanSize_data() {
        QTest::addColumn<qint64>("bytes");
        QTest::addColumn<QString>("expected");
        QTest::newRow("0")    << qint64(0)       << "0 B";
        QTest::newRow("512")  << qint64(512)      << "512 B";
        QTest::newRow("1KB")  << qint64(1024)     << "1.0 KB";
        QTest::newRow("1MB")  << qint64(1048576)  << "1.00 MB";
    }

    void testHumanSize() {
        QFETCH(qint64, bytes);
        QFETCH(QString, expected);
        QCOMPARE(FileUtils::humanSize(bytes), expected);
    }

    void testJoinPath() {
        QCOMPARE(FileUtils::joinPath("/home/user", "docs"), "/home/user/docs");
        QCOMPARE(FileUtils::joinPath("/home/user/", "docs"), "/home/user/docs");
    }

    void testNormalizePath() {
        QCOMPARE(FileUtils::normalizePath("//home//user/"), "/home/user");
        QCOMPARE(FileUtils::normalizePath("/"), "/");
    }

    void testFileName() {
        QCOMPARE(FileUtils::fileName("/home/user/file.txt"), "file.txt");
        QCOMPARE(FileUtils::fileName("file.txt"), "file.txt");
    }

    void testDirName() {
        QCOMPARE(FileUtils::dirName("/home/user/file.txt"), "/home/user");
        QCOMPARE(FileUtils::dirName("/file.txt"), "/");
    }
};

QTEST_MAIN(TestFileUtils)
#include "test_file_utils.moc"
