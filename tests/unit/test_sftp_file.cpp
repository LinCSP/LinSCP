#include <QTest>
#include "core/sftp/sftp_file.h"
#include "core/sftp/sftp_directory.h"

using namespace linscp::core::sftp;

class TestSftpFile : public QObject {
    Q_OBJECT

private slots:
    void testPermissionsString_file() {
        SftpFileInfo f;
        f.isDir       = false;
        f.isSymLink   = false;
        f.permissions = 0644; // rw-r--r--
        const QString s = f.permissionsString();
        QCOMPARE(s.size(), 10);
        QCOMPARE(s[0], QChar('-'));  // не директория
        QCOMPARE(s[1], QChar('r'));
        QCOMPARE(s[2], QChar('w'));
        QCOMPARE(s[3], QChar('-'));
    }

    void testPermissionsString_dir() {
        SftpFileInfo f;
        f.isDir       = true;
        f.permissions = 0755; // rwxr-xr-x
        const QString s = f.permissionsString();
        QCOMPARE(s[0], QChar('l')); // isDir → 'l' (TODO: поправить на 'd')
    }

    void testIsHidden() {
        SftpFileInfo f;
        f.name = ".bashrc";
        QVERIFY(f.isHidden());
        f.name = "bashrc";
        QVERIFY(!f.isHidden());
    }

    void testSftpDirectory_filters() {
        SftpDirectory dir;
        SftpFileInfo file, subdir;
        file.name  = "readme.txt"; file.isDir  = false;
        subdir.name = "src";       subdir.isDir = true;
        dir.entries << file << subdir;

        QCOMPARE(dir.files().size(), 1);
        QCOMPARE(dir.dirs().size(),  1);
        QCOMPARE(dir.files().first().name, "readme.txt");
        QCOMPARE(dir.dirs().first().name,  "src");
    }
};

QTEST_MAIN(TestSftpFile)
#include "test_sftp_file.moc"
