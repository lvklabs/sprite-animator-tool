// Unit tests for TransitionTabController.
//
// D4.4: the "Test Transitions" tab's "Add" path used to silently drop
// state mutations on the floor (the addTrans() slot contained only a
// "// TODO: m_state->addAniTrans(aniId);" comment with no fallback) --
// this test pins down the current preview-only behaviour:
//
//   - The "Add" tool button is disabled at controller construction so
//     users do not click it expecting persistence.
//   - The button still carries a tooltip explaining the preview-only
//     status.
//   - Calling addTrans(aniId) directly populates the transition table
//     (so the preview pane still works) without touching SpriteState
//     (i.e. animations() / frames() / images() remain unchanged).
//   - removeAllTrans clears the table cleanly.
//
// When SpriteState::addAniTrans(Id) lands, the disable + dialog can be
// removed and this test extended to assert the model side too.

#include <QtTest/QtTest>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QScopeGuard>
#include <QSettings>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

#include "mainwindow.h"
#include "spritestate2.h"
#include "controllers/TransitionTabController.h"
#include "ui_mainwindow.h"

namespace {
// Dismiss any modal info dialog the controller raises so the test
// thread doesn't deadlock inside QDialog::exec(). The infoDialog helper
// in dialogs.h calls QMessageBox::exec(), which spins a local event
// loop -- we poll on a singleShot timer that closes the active modal
// the moment it appears.
void scheduleModalCloser()
{
    // Use a QTimer owned by the application so the lambda survives
    // beyond this scope. It auto-deletes once a modal has been closed.
    QTimer *t = new QTimer(QCoreApplication::instance());
    t->setInterval(20);
    QObject::connect(t, &QTimer::timeout, t, [t]() {
        if (QWidget *modal = QApplication::activeModalWidget()) {
            modal->close();
            t->stop();
            t->deleteLater();
        }
    });
    t->start();
}
}

class TestTransitionTabController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testAddButtonDisabledAtConstruction();
    void testAddTransPopulatesUiTableButNotState();
    void testRemoveAllTransClearsTable();

private:
    QString examplesDir() const;
};

void TestTransitionTabController::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("LvkLabsTest"));
    QCoreApplication::setApplicationName(QStringLiteral("LvkSpriteEditorTest"));
    QSettings s;
    s.setValue(QStringLiteral("ui/showAboutOnStartup"), false);
    s.sync();
}

QString TestTransitionTabController::examplesDir() const
{
    return QDir::currentPath() + QDir::separator() + QStringLiteral("examples");
}

void TestTransitionTabController::testAddButtonDisabledAtConstruction()
{
    // No fixture needed -- the controller disables the button in
    // wireSignals() which MainWindow calls during its own construction.
    MainWindow mw;

    QToolButton *addBtn = mw.uiPtr()->addAniTransButton;
    QVERIFY(addBtn != nullptr);
    QVERIFY2(!addBtn->isEnabled(),
             "addAniTransButton must be disabled while addAniTrans() is "
             "unimplemented on SpriteState.");
    QVERIFY2(!addBtn->toolTip().isEmpty(),
             "addAniTransButton needs an explanatory tooltip so users "
             "understand why it is disabled.");
}

void TestTransitionTabController::testAddTransPopulatesUiTableButNotState()
{
    // Load mario.lvks so the controller has real animations to add to
    // the table; relative image paths resolve from examples/.
    const QString src = examplesDir() + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    const QString savedCwd = QDir::currentPath();
    auto restoreCwd = qScopeGuard([savedCwd]() { QDir::setCurrent(savedCwd); });
    QVERIFY(QDir::setCurrent(examplesDir()));

    MainWindow mw;
    QVERIFY(mw.openFile(src));

    TransitionTabController *trans = mw.transitions();
    QVERIFY(trans != nullptr);

    // Snapshot the counts that addAniTrans() would (eventually) touch.
    const int aniCountBefore = mw.state().animations().size();
    const int imgCountBefore = mw.state().images().size();
    const int frmCountBefore = mw.state().frames().size();
    QVERIFY(aniCountBefore > 0); // sanity: mario.lvks has animations

    // mario.lvks's animation Ids start at 0; pick whichever is first to
    // avoid a hardcoded magic number.
    const Id firstAniId = mw.state().animations().begin().key();

    QTableWidget *table = mw.uiPtr()->transTableWidget;
    QVERIFY(table != nullptr);
    const int rowsBefore = table->rowCount();

    // Drive the controller directly -- the button is disabled so a
    // simulated click would be a no-op, but the slot itself is what
    // production code-paths reach via addTransDialog(). The slot pops
    // a modal "Coming soon" infoDialog (D4.4), so we arm a timer to
    // close it as soon as it appears; otherwise QMessageBox::exec()
    // would deadlock the test thread.
    scheduleModalCloser();
    trans->addTrans(firstAniId);

    // Preview-side effect: the table grew by one row.
    QCOMPARE(table->rowCount(), rowsBefore + 1);

    // Model-side invariant: nothing changed. Until addAniTrans() lands
    // on SpriteState, the controller must not silently mutate other
    // collections as a workaround.
    QCOMPARE(mw.state().animations().size(), aniCountBefore);
    QCOMPARE(mw.state().images().size(),     imgCountBefore);
    QCOMPARE(mw.state().frames().size(),     frmCountBefore);
    QVERIFY2(!mw.state().hasUnsavedChanges(),
             "Adding a (preview-only) transition must not dirty the "
             "unsaved-changes flag -- otherwise users get spurious "
             "save prompts after using a feature that does not persist.");
}

void TestTransitionTabController::testRemoveAllTransClearsTable()
{
    const QString src = examplesDir() + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    const QString savedCwd = QDir::currentPath();
    auto restoreCwd = qScopeGuard([savedCwd]() { QDir::setCurrent(savedCwd); });
    QVERIFY(QDir::setCurrent(examplesDir()));

    MainWindow mw;
    QVERIFY(mw.openFile(src));

    TransitionTabController *trans = mw.transitions();
    QVERIFY(trans != nullptr);

    const Id firstAniId = mw.state().animations().begin().key();
    // Use addTrans_ui directly so this test exercises the table-state
    // path without spending time on two modal dialogs in a row. The
    // dialog policy itself is covered by the test above.
    trans->addTrans_ui(firstAniId);
    trans->addTrans_ui(firstAniId);

    QTableWidget *table = mw.uiPtr()->transTableWidget;
    QVERIFY(table->rowCount() >= 2);

    trans->removeAllTrans();
    QCOMPARE(table->rowCount(), 0);
}

QTEST_MAIN(TestTransitionTabController)
#include "tst_transition_tab_controller.moc"
