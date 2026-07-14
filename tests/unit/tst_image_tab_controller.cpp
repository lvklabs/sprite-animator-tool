// Unit tests for ImageTabController.
//
// Phase 5b: ImageTabController was extracted out of the MainWindow god
// class in Phase 3 but has no direct test coverage. This file exercises
// the add-image / remove-image path through the controller's public API,
// asserting against the underlying SpriteState2 (which is the source of
// truth the controller mutates) and against the imgTableWidget row count
// (which is what the user actually sees).
//
// We construct a real MainWindow so the controller has a fully-wired Ui
// + state to mutate. The ctor opens a modal About dialog by default --
// disabled here via QSettings -- and otherwise runs cleanly under the
// offscreen QPA.
//
// We deliberately call addImage(InputImage)/removeImage(row) directly:
// the *Dialog slots (addImageDialog / removeSelImage) pop QFileDialog /
// QMessageBox and would block the test loop.

#include <QtTest/QtTest>
#include <QApplication>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTableWidget>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "controllers/ImageTabController.h"
#include "controllers/FrameTabController.h"
#include "spritestate2.h"
#include "inputimage.h"
#include "lvkframe.h"

class TestImageTabController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testAddImageIncrementsStateAndTable();
    void testAddTwoImagesIdsAreSequential();
    void testRemoveImageDropsStateAndTable();
    void testRemoveImageCascadesAllDependentFrames();
    void testSelectedImgIdAfterAdd();

private:
    QString examplesDir() const;
    QString fixtureImagePath() const;
};

void TestImageTabController::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("LvkLabsTest"));
    QCoreApplication::setApplicationName(QStringLiteral("LvkSpriteEditorTest"));
    QSettings s;
    s.setValue(QStringLiteral("ui/showAboutOnStartup"), false);
    s.sync();
}

QString TestImageTabController::examplesDir() const
{
    return QDir::currentPath() + QDir::separator() + QStringLiteral("examples");
}

QString TestImageTabController::fixtureImagePath() const
{
    return examplesDir() + QDir::separator() + QStringLiteral("mario1.png");
}

void TestImageTabController::testAddImageIncrementsStateAndTable()
{
    QVERIFY2(QFile::exists(fixtureImagePath()),
             qPrintable("missing fixture image: " + fixtureImagePath()));

    MainWindow mw;
    ImageTabController* images = mw.images();
    QVERIFY(images != nullptr);

    QCOMPARE(mw.state().images().size(), 0);
    QCOMPARE(mw.uiPtr()->imgTableWidget->rowCount(), 0);

    InputImage img(NullId, fixtureImagePath(), 1.0);
    const Id newId = images->addImage(img);
    QVERIFY(newId != NullId);

    QCOMPARE(mw.state().images().size(), 1);
    QCOMPARE(mw.uiPtr()->imgTableWidget->rowCount(), 1);
    QVERIFY(mw.state().images().contains(newId));
    // addImage() stores paths relative to the CWD (the .lvks format only
    // round-trips relative paths), so the absolute fixture path comes
    // back as "examples/mario1.png".
    QCOMPARE(mw.state().const_image(newId).filename,
             QStringLiteral("examples") + QDir::separator()
                 + QStringLiteral("mario1.png"));
}

void TestImageTabController::testAddTwoImagesIdsAreSequential()
{
    MainWindow mw;
    ImageTabController* images = mw.images();
    QVERIFY(images != nullptr);

    const Id id0 = images->addImage(InputImage(NullId, fixtureImagePath(), 1.0));
    const Id id1 = images->addImage(InputImage(NullId, fixtureImagePath(), 1.0));
    QCOMPARE(mw.state().images().size(), 2);
    QCOMPARE(mw.uiPtr()->imgTableWidget->rowCount(), 2);
    // Sequential id allocation is part of SpriteState's contract -- we
    // verify the controller respects it (doesn't reuse / collide).
    QVERIFY(id0 != id1);
}

void TestImageTabController::testRemoveImageDropsStateAndTable()
{
    MainWindow mw;
    ImageTabController* images = mw.images();
    QVERIFY(images != nullptr);

    images->addImage(InputImage(NullId, fixtureImagePath(), 1.0));
    QCOMPARE(mw.state().images().size(), 1);
    QCOMPARE(mw.uiPtr()->imgTableWidget->rowCount(), 1);

    // Call removeImage(row) directly: removeSelImage() pops a yesNoDialog
    // we cannot dismiss non-interactively.
    images->removeImage(0);

    QCOMPARE(mw.state().images().size(), 0);
    QCOMPARE(mw.uiPtr()->imgTableWidget->rowCount(), 0);
}

void TestImageTabController::testRemoveImageCascadesAllDependentFrames()
{
    MainWindow mw;
    ImageTabController* images = mw.images();
    FrameTabController* frames = mw.frames();
    QVERIFY(images != nullptr);
    QVERIFY(frames != nullptr);

    const Id imgId = images->addImage(InputImage(NullId, fixtureImagePath(), 1.0));
    QVERIFY(imgId != NullId);

    // Three CONSECUTIVE frame rows on the same image -- the normal case
    // for frames cut from one sprite sheet. Round 7 regression: the
    // cascade loop iterated forward over the table while removeFrame()
    // shifted later rows up, so rows 1 and 3 of a 0..4 run survived as
    // orphans referencing the deleted image (and were written to disk
    // with a dangling imgId).
    frames->addFrame(LvkFrame(NullId, imgId, 0, 0, 8, 8, QStringLiteral("a")));
    frames->addFrame(LvkFrame(NullId, imgId, 8, 0, 8, 8, QStringLiteral("b")));
    frames->addFrame(LvkFrame(NullId, imgId, 0, 8, 8, 8, QStringLiteral("c")));
    QCOMPARE(mw.state().frames().size(), 3);
    QCOMPARE(mw.uiPtr()->framesTableWidget->rowCount(), 3);

    images->removeImage(0);

    QCOMPARE(mw.state().images().size(), 0);
    QVERIFY2(mw.state().frames().isEmpty(),
             qPrintable(QStringLiteral("%1 orphan frame(s) survived the image removal")
                            .arg(mw.state().frames().size())));
    QCOMPARE(mw.uiPtr()->framesTableWidget->rowCount(), 0);
}

void TestImageTabController::testSelectedImgIdAfterAdd()
{
    MainWindow mw;
    ImageTabController* images = mw.images();
    QVERIFY(images != nullptr);

    // With no rows, the selection helper must return NullId (and not
    // crash dereferencing a nonexistent row).
    QCOMPARE(images->selectedImgId(), NullId);

    const Id newId = images->addImage(InputImage(NullId, fixtureImagePath(), 1.0));
    // addImage_ui calls setCurrentItem on the row it just added, so
    // selectedImgId should now match.
    QCOMPARE(images->selectedImgId(), newId);
}

QTEST_MAIN(TestImageTabController)
#include "tst_image_tab_controller.moc"
