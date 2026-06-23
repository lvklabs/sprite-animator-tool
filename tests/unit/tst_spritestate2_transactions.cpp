// Unit tests for SpriteState2 transaction depth-counting and undo/redo
// termination guards (Team H1.2).
//
// Two regressions covered:
//
//   1. Nested startTransaction/endTransaction must collapse into a single
//      pair of markers. Without the depth counter, re-entrant callers
//      stamped multiple st_transactionStart markers into the ring buffer
//      and undo() stopped at the innermost one -- leaving outer
//      transaction changes half-undone.
//
//   2. undo()/redo() must terminate even when a transaction-marker pair
//      has been broken by ring-buffer eviction (the older half of the
//      pair rolled off when newer states pushed in). Without the
//      iteration cap, prevState()/nextState() pinned at _first and the
//      loop ran forever -- locking the UI thread.

#include <QtTest/QtTest>
#include <QApplication>

#include "spritestate2.h"
#include "statecircularbuffer.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

class TestSpriteState2Transactions : public QObject
{
    Q_OBJECT

private slots:
    void testNestedTransactionsCollapseToSinglePair();
    void testUnderflowEndTransactionIsNoOp();
    void testUndoOnEvictedTransactionTerminates();
    void testRedoOnEvictedTransactionTerminates();
};

void TestSpriteState2Transactions::testNestedTransactionsCollapseToSinglePair()
{
    // A nested start/end pair must not write extra markers into the
    // history. Validate by reproducing the failure mode the depth counter
    // is meant to fix: do an outer transaction that brackets two adds,
    // with a re-entrant inner start/end in the middle. After undo, both
    // adds must be reverted (i.e. the entire outer transaction undoes
    // atomically). Pre-fix, undo() would stop at the inner marker and
    // leave the first add in place.
    SpriteState2 s;

    LvkFrame f1, f2;
    f1.w = 10; f1.h = 10;
    f2.w = 20; f2.h = 20;

    s.startTransaction();      // depth 0 -> 1, pushes marker
    s.addFrame(f1);            // f1.id assigned
    s.startTransaction();      // depth 1 -> 2, NO marker (was buggy: pushed)
    s.addFrame(f2);            // f2.id assigned
    s.endTransaction();        // depth 2 -> 1, NO marker (was buggy: pushed)
    s.endTransaction();        // depth 1 -> 0, pushes end marker

    QCOMPARE(s.frames().size(), 2);
    QVERIFY(s.canUndo());

    // Single undo of the outer transaction must roll back BOTH frames.
    s.undo();
    QCOMPARE(s.frames().size(), 0);
}

void TestSpriteState2Transactions::testUnderflowEndTransactionIsNoOp()
{
    // A stray endTransaction with no matching start must not push an end
    // marker (which would confuse the undo walker). With the depth guard
    // it should clamp at zero and be a no-op.
    SpriteState2 s;
    LvkFrame f;
    s.addFrame(f);
    QVERIFY(s.canUndo());

    // No active transaction; this should be a defensive no-op rather than
    // poisoning the buffer with an unpaired marker.
    s.endTransaction();

    // The single frame add is still the most recent change; one undo
    // should remove it cleanly without spinning on an orphan end marker.
    s.undo();
    QCOMPARE(s.frames().size(), 0);
}

void TestSpriteState2Transactions::testUndoOnEvictedTransactionTerminates()
{
    // Build a transaction that's too large to fit in the ring buffer:
    // open a transaction, push more change records than BUFF_SIZE so the
    // st_transactionStart marker gets evicted, then close. The buffer now
    // holds an st_transactionEnd whose matching start has rolled off.
    //
    // Pre-fix: undo() would walk back to _first, see currentState() pinned
    // there, never find st_transactionStart, and loop forever -- hanging
    // the UI. Post-fix: the iteration cap at BUFF_SIZE breaks the loop
    // with a warning.
    SpriteState2 s;
    const int overflow = StateCircularBuffer::BUFF_SIZE + 10;

    s.startTransaction();
    for (int i = 0; i < overflow; ++i) {
        LvkFrame f;
        // Vary geometry so addState doesn't coalesce.
        f.w = i + 1;
        f.h = i + 1;
        s.addFrame(f);
    }
    s.endTransaction();

    // The real assertion is "this call returns" -- pre-fix it hangs.
    // Run undo() and assert it terminates without QTest hitting its
    // default timeout (5 minutes is harness default; we'd never wait
    // that long if it were truly infinite, but the assertion below proves
    // we got past the call at all).
    bool result = s.undo();
    QVERIFY(result);
    // Implicit assertion: we made it past undo() without hanging.
}

void TestSpriteState2Transactions::testRedoOnEvictedTransactionTerminates()
{
    // Mirror of the undo test for the redo direction. Build an oversized
    // transaction so the st_transactionEnd is the one that gets evicted
    // (we keep the st_transactionStart in scope by adding more state
    // after closing the transaction is impossible -- once end is pushed
    // it's recorded). Simpler approach: build the oversized transaction,
    // undo past it (which itself triggers the termination guard), then
    // redo -- redo() walks forward and must also terminate even if it
    // can't find the paired marker.
    SpriteState2 s;
    const int overflow = StateCircularBuffer::BUFF_SIZE + 10;

    s.startTransaction();
    for (int i = 0; i < overflow; ++i) {
        LvkFrame f;
        f.w = i + 1;
        f.h = i + 1;
        s.addFrame(f);
    }
    s.endTransaction();

    s.undo();

    // If undo's termination guard fired, redo() walking forward into the
    // same broken transaction must also terminate via its own cap.
    if (s.canRedo()) {
        bool result = s.redo();
        QVERIFY(result);
    }
    // Implicit assertion: we made it past redo() without hanging.
}

QTEST_MAIN(TestSpriteState2Transactions)
#include "tst_spritestate2_transactions.moc"
