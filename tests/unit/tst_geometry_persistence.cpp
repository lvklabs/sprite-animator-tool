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
#include <QSettings>
#include <QSize>

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
    void init();
    void cleanup();

    void geometryRoundtripsAcrossMainWindowInstances();

private:
    void clearGeometrySettings();
};

void TestGeometryPersistence::initTestCase() {
    QCoreApplication::setOrganizationName(QStringLiteral(LVK_TEST_ORG));
    QCoreApplication::setApplicationName(QStringLiteral(LVK_TEST_APP));
    QSettings s;
    s.setValue(QStringLiteral("ui/showAboutOnStartup"), false);
    s.sync();
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
    {
        MainWindow mw1;
        // QMainWindow needs to be shown for the platform window to honor
        // resize() on some QPA backends (offscreen included): an unshown
        // window has its geometry collapsed to defaults until first show().
        mw1.show();
        QApplication::processEvents();
        mw1.resize(target);
        QApplication::processEvents();

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
    // saw at close. Allow a 2px tolerance per dimension: window manager
    // decoration insets / fractional DPI can shift the inner content
    // rect by a pixel or two even when the persisted bytes are byte-
    // identical.
    {
        MainWindow mw2;
        mw2.show();
        QApplication::processEvents();
        const QSize got = mw2.size();
        // The persisted bytes should bring us back to within ~2px of the
        // original. Some QPA backends round the frame insets, so we
        // tolerate up to 2 pixels in each dimension.
        QVERIFY2(qAbs(got.width() - target.width()) <= 2,
                 qPrintable(QStringLiteral("width drift: target=%1 got=%2")
                                .arg(target.width()).arg(got.width())));
        QVERIFY2(qAbs(got.height() - target.height()) <= 2,
                 qPrintable(QStringLiteral("height drift: target=%1 got=%2")
                                .arg(target.height()).arg(got.height())));
    }
}

QTEST_MAIN(TestGeometryPersistence)
#include "tst_geometry_persistence.moc"
