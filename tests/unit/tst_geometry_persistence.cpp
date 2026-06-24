// Unit tests for MainWindow geometry persistence (Team H5.5).
//
// Phase 6a wired QSettings to ui/mainwindow/geometry so the editor's size
// and placement survive restarts. The roundtrip looks like this:
//
//   MainWindow A: ctor reads settings.value("ui/mainwindow/geometry")
//                  -> restoreGeometry() applies it (or falls back to the
//                     80%-of-screen heuristic)
//                  -> user resize()s the window
//                  -> closeEvent() calls settings.setValue(..., saveGeometry())
//   MainWindow B: ctor reads the persisted bytes and the window comes up
//                 at the same size + position.
//
// If saveGeometry() / restoreGeometry() ever drifts (e.g. a refactor
// reorders the closeEvent gating so the persistence write is skipped)
// every user's window would silently reset to defaults on the next
// launch. That regression slipped past CI before because no test invoked
// the close handler.

#include <QtTest/QtTest>
#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QMainWindow>
#include <QSettings>
#include <QSize>
#include <QStandardPaths>

#include "mainwindow.h"

namespace {
// Use a TEST-specific QSettings org/app so this test won't read or clobber
// the user's actual editor geometry. Macros (not constexpr auto) so
// QStringLiteral can fold them into compile-time u16 storage.
#define LVK_TEST_ORG "LvkLabsTest"
#define LVK_TEST_APP "LvkSpriteEditorTest_Geometry"
}

class TestGeometryPersistence : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void geometryRoundtripsAcrossMainWindowInstances();

private:
    void clearGeometrySettings();
};

void TestGeometryPersistence::initTestCase() {
    // Route QSettings to a per-test scratch dir so we do not leak
    // ~/.config/LvkLabsTest/LvkSpriteEditorTest_Geometry.conf into
    // the developer's home directory just because they ran `ctest`
    // locally once. Must be set BEFORE any QSettings instance is
    // constructed (including the s.setValue call below).
    QStandardPaths::setTestModeEnabled(true);

    QCoreApplication::setOrganizationName(QStringLiteral(LVK_TEST_ORG));
    QCoreApplication::setApplicationName(QStringLiteral(LVK_TEST_APP));
    QSettings s;
    s.setValue(QStringLiteral("ui/showAboutOnStartup"), false);
    s.sync();
}

void TestGeometryPersistence::cleanupTestCase() {
    // Belt-and-braces cleanup: clear() drops every key for this
    // org/app pair and unlinks the underlying QSettings backing file
    // entirely, so no residue (regardless of test-mode redirection)
    // survives the test executable.
    QSettings s;
    s.clear();
    s.sync();
    QFile::remove(s.fileName());
}

void TestGeometryPersistence::clearGeometrySettings() {
    QSettings s;
    s.remove(QStringLiteral("ui/mainwindow/geometry"));
    s.remove(QStringLiteral("ui/mainwindow/state"));
    s.sync();
}

void TestGeometryPersistence::init() {
    clearGeometrySettings();
}

void TestGeometryPersistence::cleanup() {
    clearGeometrySettings();
}

void TestGeometryPersistence::geometryRoundtripsAcrossMainWindowInstances() {
    // Stage 1: construct a MainWindow, set a known-distinctive size, then
    // simulate the accept-close path. closeEvent() persists the geometry
    // via QSettings ONLY when the unsaved-changes dialog returns non-
    // Cancel; we leave _sprState empty so hasUnsavedChanges() == false and
    // the dialog never opens.
    const QSize target(1234, 567);
    QByteArray persistedGeo;
    QSize sizeAtClose;
    {
        MainWindow mw1;
        // QMainWindow needs to be shown for the platform window to honor
        // resize() on some QPA backends (offscreen included): an unshown
        // window has its geometry collapsed to defaults until first show().
        mw1.show();
        QApplication::processEvents();
        mw1.resize(target);
        QApplication::processEvents();
        // Record the actual size mw1 settled on (may differ from `target`
        // by a pixel or two if the QPA backend snaps to a grid). This is
        // the size mw2 must reproduce -- BYTE-EQUALLY -- after restore.
        sizeAtClose = mw1.size();

        // closeEvent() persists geometry/state ONLY when hasUnsavedChanges
        // is false (no dialog) AND the event is accepted. mw1 is a fresh
        // editor with empty _sprState, so hasUnsavedChanges() is false.
        // We DO NOT call QApplication::processEvents() afterwards because
        // closeEvent() schedules QCoreApplication::exit(0); processing
        // that would exit the outer QTest event loop. Reading QSettings
        // doesn't need the event loop -- setValue + sync are synchronous.
        mw1.close();

        QSettings s;
        s.sync();  // make sure the closeEvent's setValue is flushed to disk
        persistedGeo = s.value(QStringLiteral("ui/mainwindow/geometry")).toByteArray();
    }
    QVERIFY2(!persistedGeo.isEmpty(),
             "closeEvent did not persist ui/mainwindow/geometry to QSettings");

    // Stage 2: a freshly constructed MainWindow MUST restore the size we
    // saw at close. We assert two things:
    //
    //   (a) Strict size equality positive control: mw2.size() AFTER
    //       restoreGeometry() must match the size mw1 had at close. The
    //       prior 2px tolerance was too lenient -- on offscreen QPA the
    //       default 80%-of-screen heuristic *could* land within 2px of
    //       1234x567 by coincidence, making the test pass vacuously even
    //       if restoreGeometry() never fired. Strict equality cannot be
    //       satisfied by the default-size codepath.
    //
    //   (b) restoreGeometry must accept the persisted bytes.
    //       restoreGeometry() returns false if the bytes are corrupt or
    //       a version it can't parse; that's a separate failure mode
    //       from "ctor never called restoreGeometry at all", but worth
    //       asserting to catch a future format-version drift.
    {
        MainWindow mw2;
        mw2.show();
        QApplication::processEvents();

        // (a) Strict size equality. If restoreGeometry didn't fire, the
        //     default-size heuristic kicks in and the size will be
        //     ~80% of the offscreen QPA's reported screen, NOT
        //     sizeAtClose. The fixed (1234, 567) target was chosen
        //     to be unusual enough that no plausible default landing
        //     spot could match it byte-for-byte.
        const QSize got = mw2.size();
        QCOMPARE(got, sizeAtClose);

        // (b) The persisted bytes themselves are well-formed enough for
        //     a fresh QMainWindow to restore from them. This catches a
        //     format-version drift in saveGeometry / restoreGeometry
        //     across Qt versions -- without this, a roundtrip that
        //     silently used the default-size fallback would still
        //     pass (a) iff sizeAtClose happens to equal the default).
        QMainWindow probe;
        QVERIFY2(probe.restoreGeometry(persistedGeo),
                 "QMainWindow::restoreGeometry rejected the persisted bytes "
                 "-- format-version drift in QSettings ui/mainwindow/geometry?");
    }
}

QTEST_MAIN(TestGeometryPersistence)
#include "tst_geometry_persistence.moc"
