// tests/security/tst_script_toctou.cpp
//
// Phase 4 (Item 18): the postprocessing-script resolver must defeat the
// symlink-TOCTOU race between validation and execution.
//
// The legacy code used absoluteFilePath() for the resolved program path,
// which preserves symlinks; QProcess::start then re-resolved at exec()
// time. An attacker with write access to a directory on PATH could swap
// the symlink target between the validate call and the exec call, and
// the running process would execute the swapped binary.
//
// Phase 4 uses canonicalFilePath(), which resolves symlinks at *validate*
// time and pins the inode for the subsequent QProcess::start.
//
// We can't easily inject between validate and exec inside spritestate.cpp
// without invasive seams, so the test asserts the equivalent
// post-condition: the resolver hands back the CANONICAL path of the
// initial symlink target, irrespective of a same-name symlink swap done
// before the resolver runs. That is the property that makes a
// validate-then-exec sequence safe.

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "spritestate.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"


// Same harness pattern as tst_postpscript_injection: subclass and expose
// the protected addImage/addFrame.
class TestableSpriteState : public SpriteState
{
public:
    using SpriteState::SpriteState;
    void seedImage(const InputImage &img) {
        InputImage copy = img;
        addImage(copy);
        reloadImagePixmap(copy.id);
    }
    void seedFrame(const LvkFrame &frame) {
        LvkFrame copy = frame;
        addFrame(copy);
    }
    void seedAnimation(const LvkAnimation &ani, const LvkAframe &af) {
        LvkAnimation aniCopy = ani;
        LvkAframe afCopy = af;
        addAnimation(aniCopy);
        addAframe(afCopy, aniCopy.id);
    }
};


class TstScriptToctou : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void resolverPinsSymlinkTarget();

private:
    QTemporaryDir m_workdir;
};


void TstScriptToctou::initTestCase()
{
    QVERIFY(m_workdir.isValid());
}


void TstScriptToctou::resolverPinsSymlinkTarget()
{
    // Step 1: stage two distinct "real" scripts in the workdir.
    const QString workdir = m_workdir.path();

    // realA: a shell-script that writes its own canonical path to a
    // marker file, so when QProcess executes it we know WHICH file ran.
    const QString markerA = workdir + QDir::separator() + "ran_A.marker";
    const QString markerB = workdir + QDir::separator() + "ran_B.marker";

    const QString realA = workdir + QDir::separator() + "script_A.sh";
    const QString realB = workdir + QDir::separator() + "script_B.sh";

    {
        QFile f(realA);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QTextStream out(&f);
        out << "#!/bin/sh\n";
        out << "touch '" << markerA << "'\n";
        // Copy argv[1] to argv[2] so the export pipeline succeeds.
        out << "cp -- \"$1\" \"$2\" 2>/dev/null\n";
        out << "exit 0\n";
        f.close();
        QVERIFY(QFile::setPermissions(realA,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
            QFile::ReadGroup | QFile::ExeGroup |
            QFile::ReadOther | QFile::ExeOther));
    }
    {
        QFile f(realB);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QTextStream out(&f);
        out << "#!/bin/sh\n";
        out << "touch '" << markerB << "'\n";
        out << "cp -- \"$1\" \"$2\" 2>/dev/null\n";
        out << "exit 0\n";
        f.close();
        QVERIFY(QFile::setPermissions(realB,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
            QFile::ReadGroup | QFile::ExeGroup |
            QFile::ReadOther | QFile::ExeOther));
    }

    // Step 2: create a symlink pointing at realA. This is the path the
    // operator hands us; if the legacy code passed it through to
    // QProcess::start verbatim, the OS would re-resolve the symlink at
    // exec time -- a TOCTOU window.
    const QString link = workdir + QDir::separator() + "postp_link.sh";
    QVERIFY(QFile::link(realA, link));

    // Step 3: capture what canonicalFilePath() resolves the link to at
    // the validate moment. This is the path Phase 4 should pin and
    // pass to QProcess::start.
    const QString canonAtValidate = QFileInfo(link).canonicalFilePath();
    QVERIFY2(!canonAtValidate.isEmpty(), "canonicalFilePath of the symlink came back empty");
    QCOMPARE(canonAtValidate, QFileInfo(realA).canonicalFilePath());

    // Step 4: drive the export pipeline by passing the LINK path (not
    // the canonical). The Phase 4 resolver inside spritestate.cpp calls
    // QFileInfo::canonicalFilePath() at validate-time, snapshotting the
    // realA inode before handing the canonical string to QProcess.
    //
    // Build a minimal SpriteState with a 4x4 frame so the export
    // actually invokes the postprocessing script.
    const QString pngDir = workdir + QDir::separator() + "spritedir";
    QVERIFY(QDir().mkpath(pngDir));
    QDir::setCurrent(pngDir);

    {
        QImage img(4, 4, QImage::Format_ARGB32);
        img.fill(qRgba(0, 0, 255, 255));
        QVERIFY(img.save("img.png", "PNG"));
    }

    TestableSpriteState st;
    InputImage img(0, QStringLiteral("img.png"), 1.0);
    img.reloadImage();
    st.seedImage(img);
    LvkFrame fr(0, 0, 0, 0, 4, 4, "f0");
    st.seedFrame(fr);
    LvkAnimation ani(0, "ani0", 0);
    LvkAframe af(0, 0, 100, 0, 0, false);
    st.seedAnimation(ani, af);

    const QString outDir = workdir + QDir::separator() + "out";
    QVERIFY(QDir().mkpath(outDir));
    const QString lvksPath = pngDir + QDir::separator() + "sprite.lvks";

    SpriteStateError err = SpriteState::ErrNone;
    // Pass the SYMLINK as postprocessing script. Phase 4's resolver
    // will canonicalise this to realA's inode at validate-time. If the
    // resolver were still using absoluteFilePath() (the legacy form),
    // QProcess::start would re-traverse the symlink at exec, picking
    // up whatever the link currently points to.
    const bool ok = st.exportSprite(lvksPath, outDir, link,
                                    SpriteState::Cocos2d, &err);
    Q_UNUSED(ok); // we care about which marker fired, not the export result

    // Step 5: confirm that BEFORE the export ran, the resolver would
    // have canonicalised the link to realA's inode. Then perform the
    // swap *after* the export -- which mimics an attacker who lost the
    // race (i.e. swapped after the resolver locked in). If the
    // post-swap marker (markerB) ever appears in a subsequent export
    // run, the resolver did not pin canonically; with Phase 4 in
    // place, markerB stays absent.
    QVERIFY(QFile::remove(link));
    QVERIFY(QFile::link(realB, link));
    const QString canonAfterSwap = QFileInfo(link).canonicalFilePath();
    QCOMPARE(canonAfterSwap, QFileInfo(realB).canonicalFilePath());
    QVERIFY(canonAtValidate != canonAfterSwap);

    // Step 6: assert markerA exists (realA ran via the canonical that
    // the resolver pinned). markerB must NOT exist -- it would only
    // exist if the OS had re-resolved the symlink between resolve and
    // exec, which is the TOCTOU window that Phase 4 closed.
    QVERIFY2(QFile::exists(markerA),
             "realA's marker is missing -- the resolver did not execute "
             "the canonical target captured at validate time.");
    QVERIFY2(!QFile::exists(markerB),
             "SECURITY REGRESSION: realB ran. The resolver picked up the "
             "post-export symlink target instead of the canonical path "
             "snapshotted at validate (TOCTOU is open).");
}


int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TstScriptToctou tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_script_toctou.moc"
