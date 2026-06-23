// Unit tests for the LvkSpriteEditor CLI exit codes (Team H5.3).
//
// The headless --export path documents three distinct exit codes:
//   * 0  -- export completed cleanly
//   * -1 -- CLI / load failure (missing file, bad CLI args, etc.)
//   * 2  -- export ran, but load() rejected one or more image records
//           (D2.1 format whitelist) -- partial success / silent data loss
//
// None of those exit codes were exercised by ctest before this file. A
// regression that flipped the rejected-record path from "exit 2" to
// "exit 0 + warn-only" would slip past CI and surface only when a user
// piped --export into a deployment script.
//
// We spawn the actually-built LvkSpriteEditor binary via QProcess (not a
// loopback call into runHeadlessExport()) so we exercise the production
// argv -> QCommandLineParser -> runHeadlessExport flow end-to-end. The
// binary is located via the CMake-injected LVK_BIN_PATH compile-time
// macro; the working directory is the test scratch dir.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

#ifndef LVK_BIN_PATH
#  define LVK_BIN_PATH ""
#endif

#ifndef LVK_EXAMPLES_DIR
#  define LVK_EXAMPLES_DIR ""
#endif

class TestCliExitCodes : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void exportValidLvksReturnsZero();
    void exportMissingFileReturnsMinusOne();
    void exportRejectedImageReturnsTwo();

private:
    /// Path to the built LvkSpriteEditor binary. On macOS the cmake target
    /// produces a .app bundle (MACOSX_BUNDLE TRUE), so we try both layouts.
    QString binaryPath() const;

    /// Path to the examples/ dir baked in by CMake.
    QString examplesDir() const { return QString::fromUtf8(LVK_EXAMPLES_DIR); }

    /// Wrap QProcess::start with the offscreen QPA env baked in. Returns
    /// the exit code or -999 if the process never finished.
    int runEditor(const QStringList &args, QByteArray *stderrOut = nullptr,
                  QByteArray *stdoutOut = nullptr,
                  const QString &workingDir = QString()) const;
};

QString TestCliExitCodes::binaryPath() const {
    const QString configured = QString::fromUtf8(LVK_BIN_PATH);
    if (configured.isEmpty()) {
        return QString();
    }
    if (QFileInfo::exists(configured)) {
        return configured;
    }
    // macOS app-bundle layout: <build>/LvkSpriteEditor.app/Contents/MacOS/LvkSpriteEditor
    const QString appBundle = configured + QStringLiteral(".app/Contents/MacOS/")
                              + QFileInfo(configured).fileName();
    if (QFileInfo::exists(appBundle)) {
        return appBundle;
    }
    return configured;  // surface the configured path so the QVERIFY failure is informative
}

int TestCliExitCodes::runEditor(const QStringList &args, QByteArray *stderrOut,
                                QByteArray *stdoutOut, const QString &workingDir) const {
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    proc.setProcessEnvironment(env);
    if (!workingDir.isEmpty()) {
        proc.setWorkingDirectory(workingDir);
    }
    proc.start(binaryPath(), args);
    if (!proc.waitForStarted(10000)) {
        return -999;
    }
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        proc.waitForFinished(2000);
        return -999;
    }
    if (stderrOut) {
        *stderrOut = proc.readAllStandardError();
    }
    if (stdoutOut) {
        *stdoutOut = proc.readAllStandardOutput();
    }
    return proc.exitCode();
}

void TestCliExitCodes::initTestCase() {
    const QString bin = binaryPath();
    QVERIFY2(!bin.isEmpty(),
             "LVK_BIN_PATH was not provided by CMake -- can't locate the editor binary");
    QVERIFY2(QFileInfo::exists(bin),
             qPrintable(QStringLiteral("Editor binary not found at: %1").arg(bin)));
    QVERIFY2(QFileInfo(examplesDir()).isDir(),
             qPrintable(QStringLiteral("LVK_EXAMPLES_DIR is not a directory: %1")
                            .arg(examplesDir())));
}

void TestCliExitCodes::exportValidLvksReturnsZero() {
    // examples/mario.lvks is the canonical happy-path fixture: well-formed,
    // all referenced images have whitelisted extensions and exist on disk.
    QTemporaryDir outDir;
    QVERIFY(outDir.isValid());

    const QStringList args{
        QStringLiteral("--export"),
        QStringLiteral("mario.lvks"),
        QStringLiteral("-o"),
        outDir.path(),
        QStringLiteral("-f"),
        QStringLiteral("cocos2d"),
    };

    QByteArray err;
    const int rc = runEditor(args, &err, nullptr, examplesDir());
    QVERIFY2(rc == 0,
             qPrintable(QStringLiteral("expected exit 0 (clean export), got %1; stderr:\n%2")
                            .arg(rc).arg(QString::fromUtf8(err))));
}

void TestCliExitCodes::exportMissingFileReturnsMinusOne() {
    // A non-existent input is a hard CLI failure (we never get to the
    // exporter's "partial success" logic). main.cpp::runHeadlessExport
    // returns -1; QProcess surfaces that as exit code 255 (unsigned 8-bit
    // wrap of -1). Accept either form: ctest runs us under the host
    // shell's exit-code encoding.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString fakeFile = tmpDir.path() + QDir::separator()
                             + QStringLiteral("definitely_does_not_exist.lvks");
    QVERIFY(!QFile::exists(fakeFile));

    const QStringList args{
        QStringLiteral("--export"),
        fakeFile,
        QStringLiteral("-o"),
        tmpDir.path(),
    };

    QByteArray err;
    const int rc = runEditor(args, &err);
    // Process exit codes are unsigned on POSIX; -1 surfaces as 255.
    QVERIFY2(rc == -1 || rc == 255,
             qPrintable(QStringLiteral("expected exit -1/255 (missing file), got %1; stderr:\n%2")
                            .arg(rc).arg(QString::fromUtf8(err))));
}

void TestCliExitCodes::exportRejectedImageReturnsTwo() {
    // Build a synthetic .lvks whose image record points at a file with a
    // non-whitelisted extension (.eps). The validator (lvk::validateImageFile
    // under ExistenceCheck::Optional) rejects the record on extension alone
    // even when the file is absent, so we don't need to drop an actual eps
    // file in the scratch dir -- the rejection still happens at load time
    // and runHeadlessExport returns exit code 2.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString lvks = tmpDir.path() + QDir::separator() + QStringLiteral("rejected.lvks");
    {
        QFile f(lvks);
        QVERIFY(f.open(QFile::WriteOnly | QFile::Text));
        QTextStream ts(&f);
        ts << "### LvkSprite ##\n";
        ts << "LvkSprite version 0.4\n\n";
        ts << "images(\n";
        // .eps is NOT on the format whitelist (png/jpg/jpeg/bmp/gif/webp/
        // xpm/xbm/tif/tiff). Even though InputImage::fromString accepts
        // the path as syntactically safe, the loader's whitelist check
        // skips the record and bumps rejectedCount, which makes
        // runHeadlessExport return exit code 2.
        ts << "\t0,evil.eps,1\n";
        ts << ")\n\n";
        ts << "frames(\n";
        ts << ")\n\n";
        ts << "animations(\n";
        ts << ")\n\n";
    }

    const QStringList args{
        QStringLiteral("--export"),
        lvks,
        QStringLiteral("-o"),
        tmpDir.path(),
        QStringLiteral("-f"),
        QStringLiteral("json"),
    };

    QByteArray err;
    const int rc = runEditor(args, &err);
    QVERIFY2(rc == 2,
             qPrintable(QStringLiteral("expected exit 2 (rejected record), got %1; stderr:\n%2")
                            .arg(rc).arg(QString::fromUtf8(err))));
    QVERIFY2(err.contains("rejected"),
             qPrintable(QStringLiteral("expected stderr to mention 'rejected'; got:\n%1")
                            .arg(QString::fromUtf8(err))));
}

QTEST_MAIN(TestCliExitCodes)
#include "tst_cli_exit_codes.moc"
