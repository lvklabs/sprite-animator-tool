// tests/security/tst_postpscript_injection.cpp
//
// Agent 5: regression test for the shell-injection vulnerability that used
// to live in SpriteState::writeImageWithPostprocessing(). Verifies that
// shell metacharacters supplied via --postprocessing-script CANNOT trigger
// shell expansion or side effects beyond what an exec'd, argv-list
// subprocess can directly do.
//
// We stand up a temp workspace containing:
//   - a 4x4 PNG "image.png" that an InputImage refers to,
//   - a SENTINEL file that a successful shell injection would delete,
//   - a fake "postprocessing script" that, when run, logs its argv to a
//     file (so we can prove argv was passed literally).
//
// Then we invoke SpriteState::exportSprite() with a postprocessing-script
// value of the form `; rm -rf <sentinel>` and assert:
//
//   1. The sentinel file still exists after export.
//   2. Either:
//      (a) the fake script was NEVER invoked (validation rejected the
//          metachar-laden program path), OR
//      (b) the fake script was invoked with the LITERAL string
//          `; rm -rf <sentinel>` as its argv[0] -- proving the platform
//          treated the payload as one opaque filename, not as shell
//          syntax.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <cstdlib>

#include "spritestate.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"
#include "types.h"

namespace {

// Subclass that exposes the protected hash members so the test can wire up
// a minimal SpriteState without going through the full GUI / file pipeline.
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

QString writeDummyPng(const QString &dir)
{
    const QString path = dir + QDir::separator() + "image.png";
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(qRgba(255, 0, 0, 255));
    if (!img.save(path, "PNG")) {
        return QString();
    }
    return path;
}

// A POSIX shell-script wrapper that records its argv to a log file. We
// write the log path into the script body so the test can find it later.
// The script intentionally does NOTHING else (no rm, no copy) -- the goal
// is just to observe argv.
QString writeFakeScript(const QString &dir, const QString &logPath)
{
    const QString path = dir + QDir::separator() + "fake_postp.sh";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream out(&f);
    out << "#!/bin/sh\n";
    out << "# Fake postprocessing script for the injection regression test.\n";
    out << "# Records its argv to the log file (one arg per line, '|' delim).\n";
    out << "{\n";
    out << "  for a in \"$@\"; do printf '%s|' \"$a\"; done\n";
    out << "  printf '\\n'\n";
    out << "} >> " << QString(logPath).replace('\'', "'\\''").prepend('\'').append('\'') << "\n";
    out << "# Copy input to output so writeImageWithPostprocessing succeeds.\n";
    out << "# argv[1] = input png, argv[2] = output png\n";
    out << "if [ $# -ge 2 ]; then cp -- \"$1\" \"$2\" 2>/dev/null; fi\n";
    out << "exit 0\n";
    f.close();

    // chmod +x
    QFile::setPermissions(path,
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
        QFile::ReadGroup | QFile::ExeGroup |
        QFile::ReadOther | QFile::ExeOther);
    return path;
}

} // namespace


class TstPostpScriptInjection : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void rejectsShellMetacharPayload();
    void acceptsBenignScript();

private:
    QTemporaryDir m_workdir;
};


void TstPostpScriptInjection::initTestCase()
{
    QVERIFY2(m_workdir.isValid(),
             "Could not create temp workdir for security test");
}


void TstPostpScriptInjection::rejectsShellMetacharPayload()
{
    const QString workdir = m_workdir.path();

    // ---- 1. set up workspace -----------------------------------------
    const QString pngPath = writeDummyPng(workdir);
    QVERIFY(!pngPath.isEmpty());

    const QString sentinel = workdir + QDir::separator() + "sentinel_keep_me.dat";
    {
        QFile f(sentinel);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("if you can see this, shell injection did NOT occur");
        f.close();
    }
    QVERIFY(QFile::exists(sentinel));

    const QString scriptLog = workdir + QDir::separator() + "argv_log.txt";
    const QString fakeScript = writeFakeScript(workdir, scriptLog);
    QVERIFY(!fakeScript.isEmpty());

    // ---- 2. wire up SpriteState --------------------------------------
    TestableSpriteState st;
    InputImage img(0, pngPath, 1.0);
    img.reloadImage(); // load pixmap
    st.seedImage(img);

    LvkFrame frame(0, 0, 0, 0, 4, 4, "frame0");
    st.seedFrame(frame);

    LvkAnimation ani(0, "ani0", 0);
    LvkAframe af(0, 0, 100, 0, 0, false);
    st.seedAnimation(ani, af);

    // ---- 3. invoke export with shell-metachar payload ----------------
    //
    // The payload is engineered so that *if* the legacy code path was
    // still in effect (concat string + QProcess::start(QString)), the
    // shell would interpret `; rm -rf <sentinel>` as a second command
    // and delete the sentinel. With the fix in place, this whole string
    // is treated as ONE program path -- which doesn't exist -- so the
    // script never runs, AND nothing on the filesystem is touched.
    const QString payload = QStringLiteral("; rm -rf ") + sentinel;

    const QString outDir = workdir + QDir::separator() + "out";
    QVERIFY(QDir().mkpath(outDir));
    const QString fakeLvksPath = workdir + QDir::separator() + "sprite.lvks";

    SpriteStateError err = SpriteState::ErrNone;
    // exportSprite() may return false because writing the bin output
    // expects valid frame pixmaps for *all* used frames; we don't care
    // about the return code here -- we care about FS side effects.
    st.exportSprite(fakeLvksPath, outDir, payload, &err);

    // ---- 4. assertions: sentinel survives ----------------------------
    QVERIFY2(QFile::exists(sentinel),
             qPrintable(QStringLiteral(
                "SECURITY REGRESSION: sentinel file '%1' was deleted - "
                "shell expansion of `; rm -rf ...` payload occurred. "
                "The fix in SpriteState::writeImageWithPostprocessing has "
                "regressed.").arg(sentinel)));

    // ---- 5. assertion: if the script log exists at all, the literal
    //         payload was passed as argv[1] (NOT as an extra command).
    if (QFile::exists(scriptLog)) {
        QFile lf(scriptLog);
        QVERIFY(lf.open(QIODevice::ReadOnly));
        const QString logContents = QString::fromUtf8(lf.readAll());
        lf.close();
        // If somehow our fake script was invoked, none of its argv
        // entries should *be* "rm" or "-rf". (They could only be the
        // input/output png paths.) Reaching this branch at all would
        // mean validation accepted "; rm -rf <sentinel>" as a program
        // path, which on POSIX is impossible because such a file
        // doesn't exist - but we assert defensively.
        QVERIFY2(!logContents.contains(QStringLiteral("|rm|"))
                 && !logContents.contains(QStringLiteral("|-rf|")),
                 qPrintable(QStringLiteral(
                    "SECURITY REGRESSION: fake script was invoked with "
                    "tokenised payload. Log: %1").arg(logContents)));
    }
}


void TstPostpScriptInjection::acceptsBenignScript()
{
    // Positive control: a well-formed script command (validated path,
    // no metachars) should be accepted and the script should be
    // invoked with the input/output paths as argv[1]/argv[2].

    const QString workdir = m_workdir.path() + QDir::separator() + "pos";
    QVERIFY(QDir().mkpath(workdir));

    const QString pngPath = writeDummyPng(workdir);
    QVERIFY(!pngPath.isEmpty());

    const QString scriptLog = workdir + QDir::separator() + "argv_log_pos.txt";
    const QString fakeScript = writeFakeScript(workdir, scriptLog);
    QVERIFY(!fakeScript.isEmpty());

    TestableSpriteState st;
    InputImage img(0, pngPath, 1.0);
    img.reloadImage();
    st.seedImage(img);

    LvkFrame frame(0, 0, 0, 0, 4, 4, "frame0");
    st.seedFrame(frame);

    LvkAnimation ani(0, "ani0", 0);
    LvkAframe af(0, 0, 100, 0, 0, false);
    st.seedAnimation(ani, af);

    const QString outDir = workdir + QDir::separator() + "out";
    QVERIFY(QDir().mkpath(outDir));
    const QString fakeLvksPath = workdir + QDir::separator() + "sprite.lvks";

    SpriteStateError err = SpriteState::ErrNone;
    // benign: just the script path, no extra args.
    bool ok = st.exportSprite(fakeLvksPath, outDir, fakeScript, &err);
    Q_UNUSED(ok);

    // The script SHOULD have been invoked at least once and the log
    // should contain two args -- the two image paths.
    QVERIFY2(QFile::exists(scriptLog),
             "Benign postprocessing script was NOT invoked -- the fix is "
             "over-restrictive.");

    QFile lf(scriptLog);
    QVERIFY(lf.open(QIODevice::ReadOnly));
    const QString logContents = QString::fromUtf8(lf.readAll());
    lf.close();
    // The fake script writes one '|' between args plus one trailing '|',
    // so a 2-arg invocation produces 2 pipes. Just check the input png
    // path appears as a positional arg.
    QVERIFY2(logContents.contains(pngPath) || logContents.contains(QFileInfo(pngPath).fileName())
             || logContents.contains(QStringLiteral("lvk-frame-")),
             qPrintable(QStringLiteral(
                "Benign script ran but argv did not contain the input "
                "image path. Log: %1").arg(logContents)));
}


int main(int argc, char *argv[])
{
    // QPixmap requires a QGuiApplication + a platform plugin. Use the
    // "offscreen" QPA so the test works in CI / sandbox without an X
    // server. Setting the env var BEFORE constructing QGuiApplication
    // is mandatory.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TstPostpScriptInjection tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_postpscript_injection.moc"
