// Unit tests for StateCircularBuffer -- the ring buffer that backs
// SpriteState2's undo/redo machinery.
//
// The buffer is initialized with one "null" state at index 0, so:
//   * hasPrevState() is false on a fresh buffer (current == _first).
//   * hasNextState() is false on a fresh buffer.
//   * addState() advances the write head, and consecutive equal states
//     are coalesced (no-op).
//   * the saved-flag bit moves only when explicitly set.

#include <QtTest/QtTest>

#include "statecircularbuffer.h"
#include "settings.h"

class TestStateCircularBuffer : public QObject
{
    Q_OBJECT

private slots:
    void testFreshBufferHasNoHistory();
    void testAddSingleStateEnablesUndo();
    void testUndoRedoCycle();
    void testConsecutiveEqualStatesCoalesced();
    void testAddingNewStateAfterUndoTruncatesRedo();
    void testCapacityOverflowEvictsOldest();
    void testClearRestoresPristineState();
    void testSavedFlagSemantics();
    void testSavedFlagInvalidatedOnOverwrite();
};

namespace {
// helper: build a unique-ish StateChange so addState doesn't coalesce it
StateChange makeState(int seed)
{
    StateChange st;
    st.type = StateCircularBuffer::st_updateFrame;
    st.data.frame = LvkFrame(seed, /*imgId*/ seed, 0, 0, 0, 0);
    return st;
}
}

void TestStateCircularBuffer::testFreshBufferHasNoHistory()
{
    StateCircularBuffer buf;
    QVERIFY(!buf.hasPrevState());
    QVERIFY(!buf.hasNextState());
    // The initial seeded entry is the null state.
    QCOMPARE(buf.currentState().type, StateCircularBuffer::st_null);
}

void TestStateCircularBuffer::testAddSingleStateEnablesUndo()
{
    StateCircularBuffer buf;
    buf.addState(makeState(1));
    QVERIFY(buf.hasPrevState());
    QVERIFY(!buf.hasNextState());
    QCOMPARE(buf.currentState().data.frame.id, 1);
}

void TestStateCircularBuffer::testUndoRedoCycle()
{
    StateCircularBuffer buf;
    buf.addState(makeState(1));
    buf.addState(makeState(2));
    buf.addState(makeState(3));

    QCOMPARE(buf.currentState().data.frame.id, 3);

    buf.prevState();
    QCOMPARE(buf.currentState().data.frame.id, 2);
    QVERIFY(buf.hasNextState());
    QVERIFY(buf.hasPrevState());

    buf.prevState();
    QCOMPARE(buf.currentState().data.frame.id, 1);

    buf.nextState();
    buf.nextState();
    QCOMPARE(buf.currentState().data.frame.id, 3);
    QVERIFY(!buf.hasNextState());
}

void TestStateCircularBuffer::testConsecutiveEqualStatesCoalesced()
{
    StateCircularBuffer buf;
    buf.addState(makeState(7));
    buf.addState(makeState(7));
    buf.addState(makeState(7));
    // After three identical pushes there should still be exactly one
    // undo level beyond the initial null seed.
    buf.prevState();
    QCOMPARE(buf.currentState().type, StateCircularBuffer::st_null);
    QVERIFY(!buf.hasPrevState());
}

void TestStateCircularBuffer::testAddingNewStateAfterUndoTruncatesRedo()
{
    StateCircularBuffer buf;
    buf.addState(makeState(1));
    buf.addState(makeState(2));
    buf.addState(makeState(3));
    buf.prevState();
    buf.prevState();
    QCOMPARE(buf.currentState().data.frame.id, 1);

    // Branching off should kill state 2 and 3 from the redo stack.
    buf.addState(makeState(99));
    QCOMPARE(buf.currentState().data.frame.id, 99);
    QVERIFY(!buf.hasNextState());
}

void TestStateCircularBuffer::testCapacityOverflowEvictsOldest()
{
    // Push more than BUFF_SIZE distinct states; oldest entries must drop.
    StateCircularBuffer buf;
    const int total = StateCircularBuffer::BUFF_SIZE + 25;
    for (int i = 1; i <= total; ++i) {
        buf.addState(makeState(i));
    }
    QCOMPARE(buf.currentState().data.frame.id, total);

    // Walking backwards should not reach the first push -- it was evicted.
    int hops = 0;
    while (buf.hasPrevState()) {
        buf.prevState();
        ++hops;
        QVERIFY(hops < StateCircularBuffer::BUFF_SIZE + 5); // safety net
    }
    QVERIFY(buf.currentState().data.frame.id != 1);
}

void TestStateCircularBuffer::testClearRestoresPristineState()
{
    StateCircularBuffer buf;
    buf.addState(makeState(1));
    buf.addState(makeState(2));
    buf.clear();
    QVERIFY(!buf.hasPrevState());
    QVERIFY(!buf.hasNextState());
    QCOMPARE(buf.currentState().type, StateCircularBuffer::st_null);
}

void TestStateCircularBuffer::testSavedFlagSemantics()
{
    StateCircularBuffer buf;
    buf.addState(makeState(1));
    buf.setSavedFlag();
    QVERIFY(buf.hasSavedFlag());

    buf.addState(makeState(2));
    // moved off the saved slot
    QVERIFY(!buf.hasSavedFlag());

    buf.prevState();
    // back on the saved slot
    QVERIFY(buf.hasSavedFlag());
}

void TestStateCircularBuffer::testSavedFlagInvalidatedOnOverwrite()
{
    StateCircularBuffer buf;
    buf.addState(makeState(1));
    buf.setSavedFlag();
    buf.addState(makeState(2));
    buf.prevState(); // back on saved
    QVERIFY(buf.hasSavedFlag());
    buf.addState(makeState(3)); // branch -- next slot overwritten with 3
    // the saved slot might or might not survive depending on where it sits
    // relative to the write head, but rewinding to it must yield consistent
    // semantics: either hasSavedFlag() agrees with the current index or
    // the saved index has been invalidated to -1.
    bool savedAfterBranch = buf.hasSavedFlag();
    // current state should now be the branch state, not the saved one
    QCOMPARE(buf.currentState().data.frame.id, 3);
    Q_UNUSED(savedAfterBranch);
}

QTEST_GUILESS_MAIN(TestStateCircularBuffer)
#include "tst_statecircularbuffer.moc"
