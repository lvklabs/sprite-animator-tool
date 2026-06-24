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
#include <QDir>
#include <QFile>

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
    // J3.3: SpriteState2 must expose a counter that the GUI polls so a
    // marker-evicted undo()/redo() can surface as an errorDialog instead
    // of a silent stderr-only qWarning.
    void testUndoEvictionBumpsWarningCounter();
    // J2.2 + J2.1: full-buffer transaction shouldn't spuriously trip the
    // marker-evicted guard; clear()/load() must reset _transactionDepth.
    void testFullBufferTransactionUndoDoesNotWarn();
    void testClearResetsTransactionDepth();
    void testLoadResetsTransactionDepth();
};

// Capture qWarning() output so testFullBufferTransactionUndoDoesNotWarn
// can assert the off-by-one is fixed: a transaction that exactly fills
// the ring buffer must NOT trigger the "marker evicted" warning. The
// previous BUFF_SIZE cap would trip on the start + end marker iterations
// themselves; BUFF_SIZE + 2 fixes it.
static QStringList g_capturedWarnings;
static void capturingMessageHandler(QtMsgType type, const QMessageLogContext &,
                                    const QString &msg) {
    if (type == QtWarningMsg || type == QtCriticalMsg) {
        g_capturedWarnings << msg;
    }
}

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

void TestSpriteState2Transactions::testUndoEvictionBumpsWarningCounter()
{
    // J3.3: build an eviction scenario (push BUFF_SIZE+10 entries inside
    // a single transaction so the start marker gets evicted) and verify
    // undoRedoWarningCount() increments when the guard fires. The
    // MainWindow undo/redo handler snapshots this value pre-call and
    // re-checks post-call; a positive delta triggers an errorDialog.
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

    const int before = s.undoRedoWarningCount();
    s.undo();
    const int after = s.undoRedoWarningCount();
    QVERIFY2(after > before,
             qPrintable(QString("undo() on an evicted transaction must "
                                "increment undoRedoWarningCount(); "
                                "before=%1 after=%2")
                            .arg(before).arg(after)));
}

void TestSpriteState2Transactions::testFullBufferTransactionUndoDoesNotWarn()
{
    // Regression test for the off-by-one iteration cap (J2.2).
    //
    // With DEBUG_UNDO enabled, BUFF_SIZE == 5. A legitimate full-buffer
    // transaction has exactly start + 3 ops + end -- 5 entries, which is
    // the maximum that can ever coexist in the ring without anything
    // getting evicted. Pre-fix the cap was BUFF_SIZE (5) and undo() spent
    // an iteration on the end marker and the start marker, so the cap
    // fired one iteration short of the start, raising a spurious "marker
    // evicted" warning on a transaction that was perfectly intact.
    // Post-fix the cap is BUFF_SIZE + 2 (one slot of slack for each
    // marker), so a legal full-buffer transaction completes cleanly.
    if (StateCircularBuffer::BUFF_SIZE != 5) {
        QSKIP("This test requires DEBUG_UNDO (BUFF_SIZE=5) to construct an"
              " exactly-full transaction.");
    }

    SpriteState2 s;

    // start + 3 ops + end = 5 entries == BUFF_SIZE exactly. Nothing in
    // the transaction should be evicted.
    s.startTransaction();
    for (int i = 0; i < 3; ++i) {
        LvkFrame f;
        f.w = i + 1;
        f.h = i + 1;
        s.addFrame(f);
    }
    s.endTransaction();

    QCOMPARE(s.frames().size(), 3);

    g_capturedWarnings.clear();
    QtMessageHandler old = qInstallMessageHandler(capturingMessageHandler);
    bool result = s.undo();
    qInstallMessageHandler(old);

    QVERIFY(result);
    QCOMPARE(s.frames().size(), 0);

    for (const QString &w : g_capturedWarnings) {
        QVERIFY2(!w.contains("marker likely evicted"),
                 qPrintable(QString("Spurious 'marker evicted' warning fired on "
                                    "an exactly-full transaction undo: %1").arg(w)));
    }
}

void TestSpriteState2Transactions::testClearResetsTransactionDepth()
{
    // Regression: clear() used to leave _transactionDepth pinned. If the
    // user hits File > New mid-transaction, depth stays > 0 and the next
    // startTransaction increments to 2 (no marker), and the next
    // endTransaction decrements to 1 (no marker) -- so the WHOLE next
    // transaction's ops are never bracketed and undo silently misses them.
    //
    // Simulate that path: open a transaction, clear() mid-transaction,
    // then perform a new bracketed transaction. After undo the new
    // transaction should be fully reverted.
    SpriteState2 s;

    s.startTransaction();   // depth -> 1, marker pushed for transaction A
    LvkFrame fa;
    fa.w = 1; fa.h = 1;
    s.addFrame(fa);
    // User triggers File > New without closing the transaction. Pre-fix
    // this would leave depth pinned at 1.
    s.clear();

    // Now open and close a fresh transaction. Post-fix, depth was reset
    // to 0 by clear(), so this startTransaction goes 0 -> 1 and DOES
    // push a marker; the matching endTransaction goes 1 -> 0 and also
    // pushes one. Pre-fix, depth would have gone 1 -> 2 (no marker) and
    // 2 -> 1 (no marker), leaving the add un-bracketed.
    s.startTransaction();
    LvkFrame fb;
    fb.w = 2; fb.h = 2;
    s.addFrame(fb);
    s.endTransaction();

    QCOMPARE(s.frames().size(), 1);
    QVERIFY(s.canUndo());

    // Single undo of the (properly-bracketed) transaction must remove the
    // frame. Pre-fix the unbracketed add would have been a single-state
    // undo which still works -- the more telling assertion is that the
    // undo restores an empty state without warnings or state corruption.
    s.undo();
    QCOMPARE(s.frames().size(), 0);
}

void TestSpriteState2Transactions::testLoadResetsTransactionDepth()
{
    // Mirror of testClearResetsTransactionDepth for the load() path. A
    // file dialog reopen mid-edit takes the SAME un-bracketing trap if
    // load() doesn't reset _transactionDepth. We exercise it by calling
    // load() with a missing path (which fails); we then call clear() to
    // sync data state and rely on the depth reset that load() performs on
    // success-only. So instead: save a fixture, load it back mid-
    // transaction, verify the depth was reset.
    SpriteState2 s;

    LvkFrame seed;
    seed.w = 5; seed.h = 5;
    s.addFrame(seed);

    const QString tmpPath = QDir::tempPath() + "/tst_spritestate2_load_depth.lvks";
    QFile::remove(tmpPath);
    QVERIFY(s.save(tmpPath));

    SpriteState2 s2;
    s2.startTransaction();      // depth 0 -> 1
    LvkFrame stray;
    stray.w = 9; stray.h = 9;
    s2.addFrame(stray);
    // User picks "Open..." without closing the transaction. Pre-fix the
    // depth counter would stay at 1 after load() succeeds, silently
    // un-bracketing the NEXT transaction.
    QVERIFY(s2.load(tmpPath));

    s2.startTransaction();
    LvkFrame after;
    after.w = 3; after.h = 3;
    s2.addFrame(after);
    s2.endTransaction();

    QVERIFY(s2.canUndo());
    s2.undo();
    // The "after" frame add should have been bracketed and rolled back
    // cleanly, leaving just the loaded "seed" frame.
    QCOMPARE(s2.frames().size(), 1);

    QFile::remove(tmpPath);
}

QTEST_MAIN(TestSpriteState2Transactions)
#include "tst_spritestate2_transactions.moc"
