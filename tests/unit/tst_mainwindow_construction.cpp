// Unit tests for MainWindow construction and file loading.
//
// Phase 5b: the data-class tests don't cover the MainWindow refactor surface
// (controllers + the god class split). This test exercises:
//
//   - MainWindow can be constructed in a headless environment (offscreen QPA
//     + QSettings flag to suppress the splash About dialog).
//   - The public openFile() slot loads examples/mario.lvks without crashing
//     and populates the SpriteState (images / frames / animations counts > 0).
//   - The destructor runs cleanly (no leaks, no double-free on the
//     unique_ptr-owned controllers + ui).
//
// We use a real QApplication (not QGuiApplication) because MainWindow is a
// QMainWindow that brings in Qt::Widgets. QTEST_MAIN gives us that.

#include <QtTest/QtTest>
#include <QApplication>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "mainwindow.h"
#include "spritestate2.h"

class TestMainWindowConstruction : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testConstructDestructDoesNotCrash();
    void testOpenFileLoadsMarioFixture();

private:
    QString examplesDir() const;
};

void TestMainWindowConstruction::initTestCase()
{
    // The MainWindow constructor pops a modal About QMessageBox unless this
    // QSettings key is false. Set org/app name so QSettings has a stable
    // backing scope, then disable the splash for the rest of this test
    // executable. Without this, exec() blocks the test forever.
    QCoreApplication::setOrganizationName(QStringLiteral("LvkLabsTest"));
    QCoreApplication::setApplicationName(QStringLiteral("LvkSpriteEditorTest"));
    QSettings s;
    s.setValue(QStringLiteral("ui/showAboutOnStartup"), false);
    s.sync();
}

QString TestMainWindowConstruction::examplesDir() const
{
    // ctest runs us with WORKING_DIRECTORY = ${CMAKE_SOURCE_DIR} (set by
    // lvk_add_test in tests/CMakeLists.txt). examples/ is at the repo root.
    return QDir::currentPath() + QDir::separator() + QStringLiteral("examples");
}

void TestMainWindowConstruction::testConstructDestructDoesNotCrash()
{
    // Scoped MainWindow: ASan / Qt object tree should report no leaks. The
    // ctor itself wires five controllers and the ui setup -- if any of
    // those crash this test catches it.
    {
        MainWindow mw;
        QVERIFY(mw.uiPtr() != nullptr);
        QVERIFY(mw.images()      != nullptr);
        QVERIFY(mw.frames()      != nullptr);
        QVERIFY(mw.animations()  != nullptr);
        QVERIFY(mw.transitions() != nullptr);
        QVERIFY(mw.exporter()    != nullptr);
        // The fresh state must be empty.
        QCOMPARE(mw.state().images().size(),     0);
        QCOMPARE(mw.state().frames().size(),     0);
        QCOMPARE(mw.state().animations().size(), 0);
    } // destructor runs here
}

void TestMainWindowConstruction::testOpenFileLoadsMarioFixture()
{
    const QString src = examplesDir() + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    // Load relies on relative image paths in mario.lvks; cd into examples/
    // for the duration of this test so QFile sees them.
    const QString savedCwd = QDir::currentPath();
    QVERIFY(QDir::setCurrent(examplesDir()));

    MainWindow mw;
    QVERIFY(mw.openFile(src));

    // mario.lvks: 5 input images, 6 frames, 3 animations (see tst_json_export
    // in tests/format/ for the same counts).
    QCOMPARE(mw.state().images().size(),     5);
    QVERIFY(mw.state().frames().size()      >= 1);
    QCOMPARE(mw.state().animations().size(), 3);

    QDir::setCurrent(savedCwd);
}

QTEST_MAIN(TestMainWindowConstruction)
#include "tst_mainwindow_construction.moc"
