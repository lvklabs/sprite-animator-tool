// Undo/redo operation-matrix tests for SpriteState2 (Round 7).
//
// The coverage audit found that redo() had ZERO test coverage for every
// operation type, and undo of update*/remove* was equally dark: the only
// prior undo/redo tests exercised transaction markers and termination
// guards, never the state restoration itself. This file walks every
// StateChange type through do -> undo -> redo and asserts the actual
// sprite state at each step.
//
// It also pins two Round 7 fixes:
//   * undo of an aframe removal must restore the aframe at its original
//     LIST POSITION (playback and JSON export follow in-memory order; a
//     plain append visibly reordered the animation),
//   * update*/remove* with an unknown id must be a warning no-op -- no
//     ghost entries via QMap::operator[], and no bogus StateChange whose
//     undo would re-create the ghost,
// and one behavioral contract:
//   * a GUI reorder (LvkAnimation::swapAframes) must survive a save/load
//     round-trip (ids stay position-stable; save() serializes by id).

#include <QtTest/QtTest>
#include <QList>
#include <QTemporaryDir>

#include "lvkaframe.h"
#include "lvkanimation.h"
#include "lvkframe.h"
#include "inputimage.h"
#include "spritestate2.h"

class TestSpriteState2UndoRedo : public QObject
{
    Q_OBJECT

private slots:
    void undoRedoUpdateImage();
    void undoRedoUpdateFrame();
    void undoRedoUpdateAnimation();
    void undoRedoUpdateAframe();
    void undoRedoAddRemoveImage();
    void undoRedoAddRemoveFrame();
    void undoRedoAddRemoveAnimation();
    void undoRedoAddRemoveAframe();
    void undoRestoresAframePosition();
    void unknownIdOperationsAreNoOps();
    void swapReorderSurvivesSaveLoad();

private:
    // One image (id 0), two frames (ids 0, 1), one animation (id 0)
    // with two aframes: [id 0 -> frame 0, delay 100],
    //                   [id 1 -> frame 1, delay 200].
    static void seed(SpriteState2 &st);

    static QList<Id> aframeIdOrder(const SpriteState2 &st, Id aniId);
};

void TestSpriteState2UndoRedo::seed(SpriteState2 &st)
{
    InputImage img(0, QStringLiteral("nonexistent.png"));
    st.addImage(img);

    LvkFrame f0(0, 0, 0, 0, 16, 16, QStringLiteral("f0"));
    st.addFrame(f0);
    LvkFrame f1(1, 0, 0, 0, 16, 16, QStringLiteral("f1"));
    st.addFrame(f1);

    LvkAnimation walk(0, QStringLiteral("walk"), 0);
    st.addAnimation(walk);

    LvkAframe af0(0, /*frameId=*/0, /*delay=*/100);
    st.addAframe(af0, 0);
    LvkAframe af1(1, /*frameId=*/1, /*delay=*/200);
    st.addAframe(af1, 0);
}

QList<Id> TestSpriteState2UndoRedo::aframeIdOrder(const SpriteState2 &st, Id aniId)
{
    QList<Id> ids;
    const LvkAnimation ani = st.animations().value(aniId);
    for (const LvkAframe &af : ani._aframes) {
        ids << af.id;
    }
    return ids;
}

void TestSpriteState2UndoRedo::undoRedoUpdateImage()
{
    SpriteState2 st;
    seed(st);

    st.updateImage(InputImage(0, QStringLiteral("nonexistent.png"), 2.0));
    QCOMPARE(st.const_image(0).scale(), 2.0);

    QVERIFY(st.undo());
    QCOMPARE(st.const_image(0).scale(), 1.0);

    QVERIFY(st.redo());
    QCOMPARE(st.const_image(0).scale(), 2.0);
}

void TestSpriteState2UndoRedo::undoRedoUpdateFrame()
{
    SpriteState2 st;
    seed(st);

    st.updateFrame(LvkFrame(0, 0, 4, 8, 32, 64, QStringLiteral("f0-renamed")));
    QCOMPARE(st.const_frame(0).w, 32);
    QCOMPARE(st.const_frame(0).name, QStringLiteral("f0-renamed"));

    QVERIFY(st.undo());
    QCOMPARE(st.const_frame(0).w, 16);
    QCOMPARE(st.const_frame(0).name, QStringLiteral("f0"));

    QVERIFY(st.redo());
    QCOMPARE(st.const_frame(0).w, 32);
    QCOMPARE(st.const_frame(0).name, QStringLiteral("f0-renamed"));
}

void TestSpriteState2UndoRedo::undoRedoUpdateAnimation()
{
    SpriteState2 st;
    seed(st);

    LvkAnimation renamed = st.const_animation(0);
    renamed.name = QStringLiteral("run");
    renamed.flags = 0xffffffffu;
    st.updateAnimation(renamed);
    QCOMPARE(st.const_animation(0).name, QStringLiteral("run"));
    QCOMPARE(st.const_animation(0).flags, 0xffffffffu);

    QVERIFY(st.undo());
    QCOMPARE(st.const_animation(0).name, QStringLiteral("walk"));
    QCOMPARE(st.const_animation(0).flags, 0u);

    QVERIFY(st.redo());
    QCOMPARE(st.const_animation(0).name, QStringLiteral("run"));
    QCOMPARE(st.const_animation(0).flags, 0xffffffffu);
}

void TestSpriteState2UndoRedo::undoRedoUpdateAframe()
{
    SpriteState2 st;
    seed(st);

    st.updateAframe(LvkAframe(0, /*frameId=*/1, /*delay=*/500), 0);
    QCOMPARE(st.const_aframe(0, 0).delay, 500);
    QCOMPARE(st.const_aframe(0, 0).frameId, 1);

    QVERIFY(st.undo());
    QCOMPARE(st.const_aframe(0, 0).delay, 100);
    QCOMPARE(st.const_aframe(0, 0).frameId, 0);

    QVERIFY(st.redo());
    QCOMPARE(st.const_aframe(0, 0).delay, 500);
    QCOMPARE(st.const_aframe(0, 0).frameId, 1);
}

void TestSpriteState2UndoRedo::undoRedoAddRemoveImage()
{
    SpriteState2 st;
    seed(st);

    InputImage extra(5, QStringLiteral("extra.png"));
    st.addImage(extra);
    QVERIFY(st.images().contains(5));

    QVERIFY(st.undo());
    QVERIFY(!st.images().contains(5));
    QVERIFY(st.redo());
    QVERIFY(st.images().contains(5));

    st.removeImage(5);
    QVERIFY(!st.images().contains(5));

    QVERIFY(st.undo());
    QVERIFY(st.images().contains(5));
    QCOMPARE(st.const_image(5).filename, QStringLiteral("extra.png"));
    QVERIFY(st.redo());
    QVERIFY(!st.images().contains(5));
}

void TestSpriteState2UndoRedo::undoRedoAddRemoveFrame()
{
    SpriteState2 st;
    seed(st);

    LvkFrame extra(7, 0, 0, 0, 8, 8, QStringLiteral("extra"));
    st.addFrame(extra);
    QVERIFY(st.frames().contains(7));

    QVERIFY(st.undo());
    QVERIFY(!st.frames().contains(7));
    QVERIFY(st.redo());
    QVERIFY(st.frames().contains(7));

    st.removeFrame(7);
    QVERIFY(!st.frames().contains(7));

    QVERIFY(st.undo());
    QVERIFY(st.frames().contains(7));
    QCOMPARE(st.const_frame(7).name, QStringLiteral("extra"));
    QVERIFY(st.redo());
    QVERIFY(!st.frames().contains(7));
}

void TestSpriteState2UndoRedo::undoRedoAddRemoveAnimation()
{
    SpriteState2 st;
    seed(st);

    LvkAnimation jump(3, QStringLiteral("jump"), 0);
    jump.addAframe(LvkAframe(0, 0, 150));
    st.addAnimation(jump);
    QVERIFY(st.animations().contains(3));

    QVERIFY(st.undo());
    QVERIFY(!st.animations().contains(3));
    QVERIFY(st.redo());
    QVERIFY(st.animations().contains(3));

    st.removeAnimation(3);
    QVERIFY(!st.animations().contains(3));

    QVERIFY(st.undo());
    QVERIFY(st.animations().contains(3));
    QCOMPARE(st.const_animation(3).name, QStringLiteral("jump"));
    // The animation's aframes must survive the remove+undo round-trip.
    QCOMPARE(st.animations().value(3)._aframes.size(), 1);
    QVERIFY(st.redo());
    QVERIFY(!st.animations().contains(3));
}

void TestSpriteState2UndoRedo::undoRedoAddRemoveAframe()
{
    SpriteState2 st;
    seed(st);

    LvkAframe extra(9, /*frameId=*/0, /*delay=*/300);
    st.addAframe(extra, 0);
    QVERIFY(st.const_animation(0).hasAframe(9));

    QVERIFY(st.undo());
    QVERIFY(!st.const_animation(0).hasAframe(9));
    QVERIFY(st.redo());
    QVERIFY(st.const_animation(0).hasAframe(9));

    st.removeAframe(9, 0);
    QVERIFY(!st.const_animation(0).hasAframe(9));

    QVERIFY(st.undo());
    QVERIFY(st.const_animation(0).hasAframe(9));
    QCOMPARE(st.const_aframe(0, 9).delay, 300);
    QVERIFY(st.redo());
    QVERIFY(!st.const_animation(0).hasAframe(9));
}

void TestSpriteState2UndoRedo::undoRestoresAframePosition()
{
    SpriteState2 st;
    seed(st);

    LvkAframe third(2, /*frameId=*/0, /*delay=*/300);
    st.addAframe(third, 0);
    QCOMPARE(aframeIdOrder(st, 0), (QList<Id>{0, 1, 2}));

    // Remove the MIDDLE aframe, then undo. Before the Round 7 fix the
    // undo re-appended it at the end ([0, 2, 1]), visibly reordering
    // playback in the preview and in JSON exports.
    st.removeAframe(1, 0);
    QCOMPARE(aframeIdOrder(st, 0), (QList<Id>{0, 2}));

    QVERIFY(st.undo());
    QCOMPARE(aframeIdOrder(st, 0), (QList<Id>{0, 1, 2}));
    QCOMPARE(st.const_aframe(0, 1).delay, 200);

    // Redo removes it again; a second undo restores the position again
    // (the recorded index must survive the ring-buffer round-trip).
    QVERIFY(st.redo());
    QCOMPARE(aframeIdOrder(st, 0), (QList<Id>{0, 2}));
    QVERIFY(st.undo());
    QCOMPARE(aframeIdOrder(st, 0), (QList<Id>{0, 1, 2}));
}

void TestSpriteState2UndoRedo::unknownIdOperationsAreNoOps()
{
    SpriteState2 st; // deliberately empty -- every id below is unknown

    st.updateImage(InputImage(42, QStringLiteral("ghost.png")));
    st.updateFrame(LvkFrame(42, 0, 0, 0, 8, 8, QStringLiteral("ghost")));
    st.updateAnimation(LvkAnimation(42, QStringLiteral("ghost"), 0));
    st.updateAframe(LvkAframe(42, 0, 100), /*aniId=*/999);
    st.removeImage(42);
    st.removeFrame(42);
    st.removeAnimation(42);
    st.removeAframe(42, /*aniId=*/999);

    // No ghost entries created through QMap::operator[]...
    QVERIFY(st.images().isEmpty());
    QVERIFY(st.frames().isEmpty());
    QVERIFY(st.animations().isEmpty());
    // ...and no bogus StateChange pushed (undoing one would have
    // re-created a ghost).
    QVERIFY(!st.canUndo());
}

void TestSpriteState2UndoRedo::swapReorderSurvivesSaveLoad()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    SpriteState2 st;
    seed(st);

    // Reorder via the same path the GUI uses: swap on a copy, publish
    // with updateAnimation(). swapAframes keeps ids position-stable and
    // swaps the CONTENT, so the reorder is representable in a file whose
    // aframes are serialized in id order.
    LvkAnimation ani = st.const_animation(0);
    ani.swapAframes(0, 1);
    st.updateAnimation(ani);

    // In-memory: position 0 now plays frame 1 (delay 200).
    QCOMPARE(st.const_aframe(0, 0).frameId, 1);
    QCOMPARE(st.const_aframe(0, 0).delay, 200);

    const QString out = tmpDir.path() + QStringLiteral("/reorder.lvks");
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.save(out, &err), qPrintable(SpriteState::errorMessage(err)));

    SpriteState2 reloaded;
    QVERIFY2(reloaded.load(out, &err), qPrintable(SpriteState::errorMessage(err)));
    QCOMPARE(aframeIdOrder(reloaded, 0), (QList<Id>{0, 1}));
    // The reorder survived: the first played aframe is still frame 1.
    QCOMPARE(reloaded.const_aframe(0, 0).frameId, 1);
    QCOMPARE(reloaded.const_aframe(0, 0).delay, 200);
    QCOMPARE(reloaded.const_aframe(0, 1).frameId, 0);
    QCOMPARE(reloaded.const_aframe(0, 1).delay, 100);

    // An undo after the reload-free session still restores the original
    // order in the live state.
    QVERIFY(st.undo());
    QCOMPARE(st.const_aframe(0, 0).frameId, 0);
    QCOMPARE(st.const_aframe(0, 0).delay, 100);
}

// QTEST_MAIN (not GUILESS): SpriteState carries QPixmap members, which
// require a QGuiApplication. The test runs under the offscreen QPA (see
// CMakeLists) so it still works on headless CI runners.
QTEST_MAIN(TestSpriteState2UndoRedo)
#include "tst_spritestate2_undo_redo.moc"
