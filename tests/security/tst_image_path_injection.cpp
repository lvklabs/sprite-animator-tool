// tests/security/tst_image_path_injection.cpp
//
// Phase 4 (Item 15): InputImage::fromString must reject attacker-controlled
// image paths read from a .lvks file. The on-disk format only ever carries
// relative filenames (see examples/mario.lvks); anything else is malicious.
//
// We exercise four rejection cases (absolute path, UNC prefix, ".."
// traversal, NUL byte) plus a positive case (relative path to a real on-
// disk PNG fixture) so the validator can be proven not to be a blanket
// false-positive.

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

#include "inputimage.h"
#include "types.h"

class TstImagePathInjection : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();

    void rejectsAbsolutePath();
    void rejectsUncPath();
    void rejectsDotDotTraversal();
    void rejectsEmbeddedNulByte();
    void rejectsTildePrefix();
    void rejectsWindowsDriveAbsolute();

    void acceptsRelativePathToFixture();

private:
    QTemporaryDir m_workdir;
    QString       m_fixtureRelative;
};


void TstImagePathInjection::initTestCase()
{
    QVERIFY2(m_workdir.isValid(), "Could not create temp workdir");

    // Write a 4x4 PNG and capture its CWD-relative filename. The positive
    // test expects the loaded pixmap to be non-null.
    QDir::setCurrent(m_workdir.path());
    m_fixtureRelative = QStringLiteral("fixture.png");
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(qRgba(0, 255, 0, 255));
    QVERIFY(img.save(m_fixtureRelative, "PNG"));
}


void TstImagePathInjection::rejectsAbsolutePath()
{
    // An absolute path read from a malicious .lvks is an arbitrary-read
    // primitive ("/etc/passwd", "/proc/self/environ", etc.). fromString
    // must return false and leave id == NullId.
    InputImage img;
    const bool ok = img.fromString(QStringLiteral("3,/etc/passwd,1"));
    QVERIFY2(!ok, "Absolute filename '/etc/passwd' was accepted");
    QCOMPARE(img.id, static_cast<Id>(NullId));
    QVERIFY(img.filename.isEmpty());
}


void TstImagePathInjection::rejectsUncPath()
{
    // Windows-style UNC path \\host\share. Even on Linux QPixmap won't
    // try to mount SMB, but the .lvks could be shipped to a Windows
    // operator. Reject up front.
    InputImage img;
    const bool ok = img.fromString(QStringLiteral("7,\\\\evil.example.com\\share\\bait.png,1"));
    QVERIFY2(!ok, "UNC filename was accepted");
    QCOMPARE(img.id, static_cast<Id>(NullId));
    QVERIFY(img.filename.isEmpty());
}


void TstImagePathInjection::rejectsDotDotTraversal()
{
    // ".." anywhere in the filename is a sandbox-escape signal. We
    // intentionally reject every '..' (not just leading), since a
    // mid-path '..' is a traversal too ("a/../../etc/passwd").
    InputImage img;
    const bool ok = img.fromString(QStringLiteral("12,../../etc/passwd,1"));
    QVERIFY2(!ok, "'..' traversal filename was accepted");
    QCOMPARE(img.id, static_cast<Id>(NullId));
    QVERIFY(img.filename.isEmpty());

    // Mid-path '..' too.
    InputImage img2;
    const bool ok2 = img2.fromString(QStringLiteral("13,assets/../escape.png,1"));
    QVERIFY2(!ok2, "Mid-path '..' filename was accepted");
    QCOMPARE(img2.id, static_cast<Id>(NullId));
}


void TstImagePathInjection::rejectsEmbeddedNulByte()
{
    // NUL byte truncation attack: many C-string consumers see only the
    // bytes before the first \0, while QString preserves them. A path
    // like "ok.png\0../../etc/passwd" passes a visual check but
    // dereferences to something else when handed to a system call.
    QString withNul = QStringLiteral("ok.png");
    withNul.append(QChar('\0'));
    withNul.append(QStringLiteral("/etc/passwd"));

    InputImage img;
    const QString line = QStringLiteral("44,") + withNul + QStringLiteral(",1");
    const bool ok = img.fromString(line);
    QVERIFY2(!ok, "NUL-byte-embedded filename was accepted");
    QCOMPARE(img.id, static_cast<Id>(NullId));
    QVERIFY(img.filename.isEmpty());
}


void TstImagePathInjection::rejectsTildePrefix()
{
    // Team B3: leading '~' is shell-expansion bait. Qt's QPixmap does
    // not expand it on Linux, so the read goes nowhere, but a Windows or
    // mac operator may have a shell that does. The .lvks format only
    // ever ships clean relative paths -- reject '~/...' up front.
    InputImage img;
    const bool ok = img.fromString(QStringLiteral("21,~/.ssh/id_rsa,1"));
    QVERIFY2(!ok, "Tilde-prefixed filename was accepted");
    QCOMPARE(img.id, static_cast<Id>(NullId));
    QVERIFY(img.filename.isEmpty());
}


void TstImagePathInjection::rejectsWindowsDriveAbsolute()
{
    // Team B3: Windows-drive-absolute paths like "C:\Windows\..." are
    // NOT detected as absolute by QFileInfo on a Linux runner, so the
    // pre-B3 validator let them slip through. A cross-platform .lvks
    // file shipped from Linux to Windows would then dereference an
    // attacker-supplied absolute path on the Windows host. Reject on
    // any platform.
    {
        InputImage img;
        const bool ok = img.fromString(
            QStringLiteral("22,C:\\Windows\\System32\\config\\SAM,1"));
        QVERIFY2(!ok, "Windows drive-absolute (back-slash) filename was accepted");
        QCOMPARE(img.id, static_cast<Id>(NullId));
        QVERIFY(img.filename.isEmpty());
    }
    // Also reject the forward-slash variant ("C:/foo") which some
    // Windows tools emit and Qt accepts as a path separator.
    {
        InputImage img;
        const bool ok = img.fromString(QStringLiteral("23,D:/secret/data.png,1"));
        QVERIFY2(!ok, "Windows drive-absolute (forward-slash) filename was accepted");
        QCOMPARE(img.id, static_cast<Id>(NullId));
        QVERIFY(img.filename.isEmpty());
    }
}


void TstImagePathInjection::acceptsRelativePathToFixture()
{
    // Positive control: a clean, relative path to a real on-disk PNG must
    // be accepted, the id/filename/scale must round-trip, and the pixmap
    // must load successfully (not just be a null placeholder).
    InputImage img;
    const bool ok = img.fromString(QStringLiteral("1,") + m_fixtureRelative
                                   + QStringLiteral(",1"));
    QVERIFY2(ok, "Relative fixture path was rejected by the validator");
    QCOMPARE(img.id, static_cast<Id>(1));
    QCOMPARE(img.filename, m_fixtureRelative);
    QVERIFY2(!img.pixmap.isNull(),
             "Pixmap failed to load from the fixture - the validator is "
             "blocking legitimate input.");
}


int main(int argc, char *argv[])
{
    // QPixmap needs a QGuiApplication + a platform plugin. Use offscreen
    // for headless CI.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TstImagePathInjection tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_image_path_injection.moc"
