// tst_lvks_roundtrip.cpp
//
// Golden-file round-trip test for the .lvks format.
//
// Loads the project's two committed example sprites
// (examples/mario.lvks, examples/ryu.lvks), saves the loaded SpriteState
// to a temp file, loads the temp file back, and verifies the resulting
// state is structurally equivalent to the original. This pins current
// behavior before any refactor in later phases of the upgrade.
//
// We deliberately do NOT require byte-equivalence of the on-disk file.
// In Qt6 QMap iterates sorted by key (deterministic), but the legacy
// parser is lenient (e.g. it accepts comma counts of 2/3/5/6 for
// LvkAframe::fromString and rewrites in a canonical form), so a freshly
// saved file may not be a byte match of a hand-edited v0.1 file.
//
// Structural equivalence — same number of images / frames / animations
// and per-record key fields matching — is what we lock in here.
//
// Authored by Agent 2/10.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QString>

#include "spritestate.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

#ifndef LVK_EXAMPLES_DIR
#  define LVK_EXAMPLES_DIR "."
#endif

class TstLvksRoundtrip : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void roundtripMario();
    void roundtripRyu();
    void roundtripSyntheticCanonical();
    void roundtripPreservesCustomHeader();
    void emptyStateRoundtrips();
    // J3.1: Save-As (different filename) of a transitions-bearing source
    // must NOT plant the "intentionally dropped" breadcrumb in the
    // derived file. Same-file Save must still keep the breadcrumb.
    void saveAsDropsTransitionsBreadcrumb();
    // J3.2: a transitions(...) block in the source file must bump
    // rejectedCount so the GUI and the CLI exit code surface the drop.
    void transitionsBlockBumpsRejectedCount();

private:
    // Locate a sample file. We try, in order:
    //   1. LVK_EXAMPLES_DIR compile-time macro (set by CMake)
    //   2. ./examples relative to the CWD (in case ctest's
    //      WORKING_DIRECTORY hasn't been honored)
    //   3. ../examples (developer running the binary by hand from build/)
    QString examplePath(const QString& name) const;

    // Assert that two SpriteState instances are structurally equivalent.
    // QCOMPARE-based, so failure points to the exact mismatch.
    void assertStructurallyEqual(SpriteState& a, SpriteState& b,
                                 const QString& label) const;
};

QString TstLvksRoundtrip::examplePath(const QString& name) const
{
    const QStringList candidates = {
        QString::fromUtf8(LVK_EXAMPLES_DIR) + QDir::separator() + name,
        QStringLiteral("examples") + QDir::separator() + name,
        QStringLiteral("..") + QDir::separator() + QStringLiteral("examples") + QDir::separator() + name,
    };
    for (const QString& candidate : candidates) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}

void TstLvksRoundtrip::initTestCase()
{
    // Sanity-check that the two example fixtures exist before we start.
    const QString mario = examplePath(QStringLiteral("mario.lvks"));
    const QString ryu   = examplePath(QStringLiteral("ryu.lvks"));
    QVERIFY2(!mario.isEmpty(),
             "Could not locate examples/mario.lvks (checked LVK_EXAMPLES_DIR, "
             "./examples, ../examples)");
    QVERIFY2(!ryu.isEmpty(),
             "Could not locate examples/ryu.lvks (checked LVK_EXAMPLES_DIR, "
             "./examples, ../examples)");
}

void TstLvksRoundtrip::assertStructurallyEqual(SpriteState& a, SpriteState& b,
                                               const QString& label) const
{
    qInfo() << "Comparing structures for fixture:" << label;

    // Image set: same ids, same filenames.
    const auto& imagesA = a.images();
    const auto& imagesB = b.images();
    QCOMPARE(imagesA.size(), imagesB.size());
    for (auto it = imagesA.constBegin(); it != imagesA.constEnd(); ++it) {
        QVERIFY2(imagesB.contains(it.key()),
                 qPrintable(QString("Image id %1 missing after round-trip").arg(it.key())));
        const InputImage& ia = it.value();
        const InputImage& ib = imagesB.value(it.key());
        QCOMPARE(ia.id, ib.id);
        QCOMPARE(ia.filename, ib.filename);
        // _scale survives the round-trip; defaults to 1.0 when v0.1
        // (no scale column) — and that default must round-trip.
        QCOMPARE(ia.scale(), ib.scale());
    }

    // Frame set: same ids, same geometries, same names.
    const auto& framesA = a.frames();
    const auto& framesB = b.frames();
    QCOMPARE(framesA.size(), framesB.size());
    for (auto it = framesA.constBegin(); it != framesA.constEnd(); ++it) {
        QVERIFY2(framesB.contains(it.key()),
                 qPrintable(QString("Frame id %1 missing after round-trip").arg(it.key())));
        const LvkFrame& fa = it.value();
        const LvkFrame& fb = framesB.value(it.key());
        QCOMPARE(fa.id,    fb.id);
        QCOMPARE(fa.imgId, fb.imgId);
        QCOMPARE(fa.ox,    fb.ox);
        QCOMPARE(fa.oy,    fb.oy);
        QCOMPARE(fa.w,     fb.w);
        QCOMPARE(fa.h,     fb.h);
        QCOMPARE(fa.name,  fb.name);
    }

    // Animation set: same ids, same names, same flags, and identical
    // aframe lists (id/frameId/delay/ox/oy/sticky).
    const auto& aniA = a.animations();
    const auto& aniB = b.animations();
    QCOMPARE(aniA.size(), aniB.size());
    for (auto it = aniA.constBegin(); it != aniA.constEnd(); ++it) {
        QVERIFY2(aniB.contains(it.key()),
                 qPrintable(QString("Animation id %1 missing after round-trip").arg(it.key())));
        const LvkAnimation& aa = it.value();
        const LvkAnimation& bb = aniB.value(it.key());
        QCOMPARE(aa.id,    bb.id);
        QCOMPARE(aa.name,  bb.name);
        QCOMPARE(aa.flags, bb.flags);

        QCOMPARE(aa._aframes.size(), bb._aframes.size());
        for (int i = 0; i < aa._aframes.size(); ++i) {
            const LvkAframe& fa = aa._aframes.at(i);
            const LvkAframe& fb = bb._aframes.at(i);
            QCOMPARE(fa.id,      fb.id);
            QCOMPARE(fa.frameId, fb.frameId);
            QCOMPARE(fa.delay,   fb.delay);
            QCOMPARE(fa.ox,      fb.ox);
            QCOMPARE(fa.oy,      fb.oy);
            QCOMPARE(fa.sticky,  fb.sticky);
        }
    }
}

void TstLvksRoundtrip::roundtripMario()
{
    // Agent 7 fix: SpriteState::addAframe() previously called
    // QList::insert(aframe.id, aframe) which treats aframe.id as an index
    // and was undefined behavior for non-dense ids (mario.lvks uses ids
    // 1,5,6,7 across animations). The fix changes addAframe() to append,
    // unblocking this round-trip test.
    const QString src = examplePath(QStringLiteral("mario.lvks"));
    QVERIFY(!src.isEmpty());

    SpriteState original;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(original.load(src, &err),
             qPrintable(QString("Failed to load mario.lvks: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    // Lock in current state shape — values eyeballed from the fixture.
    QCOMPARE(original.images().size(),     5);   // mario1..4 + sprites-mario
    QCOMPARE(original.frames().size(),     6);   // 0..5
    QCOMPARE(original.animations().size(), 3);   // walk / jump / queen_walk

    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "Failed to create QTemporaryDir");
    const QString out = tmpDir.path() + QDir::separator() + QStringLiteral("mario.out.lvks");

    err = SpriteState::ErrNone;
    QVERIFY2(original.save(out, &err),
             qPrintable(QString("Failed to save mario.lvks: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(QFile::exists(out));

    SpriteState reloaded;
    err = SpriteState::ErrNone;
    QVERIFY2(reloaded.load(out, &err),
             qPrintable(QString("Failed to reload saved mario.lvks: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    assertStructurallyEqual(original, reloaded, QStringLiteral("mario.lvks"));

    // A *second* save+load round must also be stable — i.e. the canonical
    // form is a fixed point of save/load.
    const QString out2 = tmpDir.path() + QDir::separator() + QStringLiteral("mario.out2.lvks");
    QVERIFY(reloaded.save(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    SpriteState reloaded2;
    QVERIFY(reloaded2.load(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    assertStructurallyEqual(reloaded, reloaded2, QStringLiteral("mario.lvks (fixpoint)"));
}

void TstLvksRoundtrip::roundtripRyu()
{
    // Agent 7 fix: same root cause as roundtripMario — SpriteState::
    // addAframe() now appends instead of QList::insert(id, …).
    const QString src = examplePath(QStringLiteral("ryu.lvks"));
    QVERIFY(!src.isEmpty());

    SpriteState original;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(original.load(src, &err),
             qPrintable(QString("Failed to load ryu.lvks: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    // Locked counts from the fixture.
    QCOMPARE(original.images().size(),     1);   // RyuCE-t.png
    QCOMPARE(original.frames().size(),     8);   // ids 0..4, 6..8
    QCOMPARE(original.animations().size(), 3);   // punch / kick / waiting

    QTemporaryDir tmpDir;
    QVERIFY2(tmpDir.isValid(), "Failed to create QTemporaryDir");
    const QString out = tmpDir.path() + QDir::separator() + QStringLiteral("ryu.out.lvks");

    err = SpriteState::ErrNone;
    QVERIFY(original.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(QFile::exists(out));

    SpriteState reloaded;
    err = SpriteState::ErrNone;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    assertStructurallyEqual(original, reloaded, QStringLiteral("ryu.lvks"));
}

void TstLvksRoundtrip::roundtripSyntheticCanonical()
{
    // Synthetic round-trip with sequential, dense aframe ids that match
    // list positions. Historically this existed to sidestep the (long
    // since fixed) addAframe id-as-index bug while roundtripMario /
    // roundtripRyu were QSKIP-ed; those golden tests now run for real,
    // and this one stays as a hermetic fixture-free complement that
    // exercises the full load + save + reload path.

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString src = tmpDir.path() + QDir::separator() + QStringLiteral("canon.lvks");
    {
        QFile f(src);
        QVERIFY(f.open(QFile::WriteOnly | QFile::Text));
        QTextStream ts(&f);
        ts << "### LvkSprite ##\n";
        ts << "LvkSprite version 0.4\n\n";
        ts << "images(\n";
        ts << "\t0,nonexistent_a.png,1\n";
        ts << "\t1,nonexistent_b.png,1\n";
        ts << ")\n\n";
        ts << "frames(\n";
        ts << "\t0,f_a,0,0,0,16,16\n";
        ts << "\t1,f_b,1,2,3,8,12\n";
        ts << "\t2,f_c,0,4,4,16,16\n";
        ts << ")\n\n";
        ts << "animations(\n";
        ts << "\t0,walk,0\n";
        ts << "\taframes(\n";
        ts << "\t\t0,0,200,0,0,0\n";
        ts << "\t\t1,1,180,4,-2,1\n";
        ts << "\t)\n";
        ts << "\t1,jump,0\n";
        ts << "\taframes(\n";
        ts << "\t\t0,2,160,0,0,0\n";
        ts << "\t\t1,0,140,0,0,0\n";
        ts << "\t)\n";
        ts << ")\n\n";
    }

    SpriteState original;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(original.load(src, &err),
             qPrintable(QString("Failed to load synthetic fixture: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(original.images().size(),     2);
    QCOMPARE(original.frames().size(),     3);
    QCOMPARE(original.animations().size(), 2);

    const QString out = tmpDir.path() + QDir::separator() + QStringLiteral("canon.out.lvks");
    QVERIFY(original.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(QFile::exists(out));

    SpriteState reloaded;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    assertStructurallyEqual(original, reloaded, QStringLiteral("synthetic"));

    // Second round must be a fixed point.
    const QString out2 = tmpDir.path() + QDir::separator() + QStringLiteral("canon.out2.lvks");
    QVERIFY(reloaded.save(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    SpriteState reloaded2;
    QVERIFY(reloaded2.load(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    assertStructurallyEqual(reloaded, reloaded2, QStringLiteral("synthetic (fixpoint)"));
}

void TstLvksRoundtrip::roundtripPreservesCustomHeader()
{
    // The v0.3+ format adds a custom_header() section. mario.lvks /
    // ryu.lvks are v0.1 (no custom header), so build an in-memory v0.4
    // fixture with a non-empty header and verify it round-trips.

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString src = tmpDir.path() + QDir::separator() + QStringLiteral("with_header.lvks");
    {
        QFile f(src);
        QVERIFY(f.open(QFile::WriteOnly | QFile::Text));
        QTextStream ts(&f);
        ts << "### LvkSprite ##\n";
        ts << "LvkSprite version 0.4\n\n";
        ts << "images(\n";
        ts << "\t0,nonexistent.png,1\n";
        ts << ")\n\n";
        ts << "frames(\n";
        ts << "\t0,solo,0,0,0,16,16\n";
        ts << ")\n\n";
        ts << "animations(\n";
        ts << "\t0,a,0\n";
        ts << "\taframes(\n";
        ts << "\t\t0,0,200,0,0,0\n";
        ts << "\t)\n";
        ts << ")\n\n";
        ts << "custom_header(\n";
        ts << "#define FOO 1\n";
        ts << "#define BAR 2\n";
        ts << ")\n\n";
    }

    SpriteState original;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(original.load(src, &err),
             qPrintable(QString("Failed to load custom-header fixture: %1")
                            .arg(SpriteState::errorMessage(err))));
    QVERIFY(!original.getCustomHeader().isEmpty());
    QVERIFY(original.getCustomHeader().contains(QStringLiteral("#define FOO 1")));

    const QString out = tmpDir.path() + QDir::separator() + QStringLiteral("with_header.out.lvks");
    QVERIFY(original.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    SpriteState reloaded;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    assertStructurallyEqual(original, reloaded, QStringLiteral("custom_header"));

    // Bug #5 FIXED by Agent 8 (May 2026): save() now writes a single
    // terminating '\n' after the trimmed header so the canonical form
    // is a fixed point. Compare strictly.
    QCOMPARE(reloaded.getCustomHeader(), original.getCustomHeader());

    // Re-save+re-load must also be a fixed point (the strongest form of
    // the assertion -- if there's *any* additive drift in save(), the
    // second round-trip will diverge from the first).
    const QString out2 = tmpDir.path() + QDir::separator() + QStringLiteral("with_header.out2.lvks");
    QVERIFY(reloaded.save(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    SpriteState reloaded2;
    QVERIFY(reloaded2.load(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(reloaded2.getCustomHeader(), reloaded.getCustomHeader());
}

void TstLvksRoundtrip::emptyStateRoundtrips()
{
    // Team H5.6: the existing fixture-based round-trips (mario.lvks /
    // ryu.lvks / synthetic) all start with non-empty image+frame+animation
    // sets. A new SpriteState with zero records is its own legitimate
    // state -- e.g. immediately after "File > New" or
    // SpriteState::clear() -- and save+load+structural-equality must
    // hold for it too. A regression that crashed save() on an empty
    // QMap, or that emitted a header-only file the loader rejected,
    // would have slipped past CI before this slot existed.

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    SpriteState empty;
    QCOMPARE(empty.images().size(),     0);
    QCOMPARE(empty.frames().size(),     0);
    QCOMPARE(empty.animations().size(), 0);

    const QString out = tmpDir.path() + QDir::separator()
                        + QStringLiteral("empty.lvks");
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(empty.save(out, &err),
             qPrintable(QString("save() failed on empty state: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(QFile::exists(out));

    SpriteState reloaded;
    err = SpriteState::ErrNone;
    QVERIFY2(reloaded.load(out, &err),
             qPrintable(QString("load() failed on empty state: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    QCOMPARE(reloaded.images().size(),     0);
    QCOMPARE(reloaded.frames().size(),     0);
    QCOMPARE(reloaded.animations().size(), 0);

    // Re-save must also be a fixed point: the canonical form of an empty
    // state is stable under save/load.
    const QString out2 = tmpDir.path() + QDir::separator()
                         + QStringLiteral("empty.out2.lvks");
    QVERIFY(reloaded.save(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    SpriteState reloaded2;
    QVERIFY(reloaded2.load(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(reloaded2.images().size(),     0);
    QCOMPARE(reloaded2.frames().size(),     0);
    QCOMPARE(reloaded2.animations().size(), 0);
}

void TstLvksRoundtrip::saveAsDropsTransitionsBreadcrumb()
{
    // J3.1: Pre-fix, _loadedTransitions was set from the LOADED file
    // and the breadcrumb was emitted on every save() that followed --
    // including Save-As to a brand-new filename. That made every
    // derived file ("File > Save As..." in the GUI, second-arg path
    // in the CLI) carry "# transitions intentionally dropped" even
    // though the derived file is a fresh document the user never
    // associated with the source's transitions data.
    //
    // The fix compares the canonical save path against the canonical
    // load path: only same-file saves inherit the flag. This test
    // covers both branches.

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Author a fixture with a transitions block. The loader recognises
    // and drops it; the breadcrumb decision is what we're locking in.
    const QString src = tmpDir.path() + QDir::separator()
                        + QStringLiteral("with_transitions.lvks");
    {
        QFile f(src);
        QVERIFY(f.open(QFile::WriteOnly | QFile::Text));
        QTextStream ts(&f);
        ts << "### LvkSprite ##\n";
        ts << "LvkSprite version 0.4\n\n";
        ts << "images(\n";
        ts << "\t0,nonexistent.png,1\n";
        ts << ")\n\n";
        ts << "frames(\n";
        ts << "\t0,solo,0,0,0,16,16\n";
        ts << ")\n\n";
        ts << "animations(\n";
        ts << "\t0,a,0\n";
        ts << "\taframes(\n";
        ts << "\t\t0,0,200,0,0,0\n";
        ts << "\t)\n";
        ts << ")\n\n";
        ts << "transitions(\n";
        ts << "\t# some content the loader will drop\n";
        ts << "\t0,0,1,100\n";
        ts << ")\n\n";
    }

    SpriteState state;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(state.load(src, &err),
             qPrintable(QString("Failed to load transitions fixture: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    auto fileContains = [](const QString &path, const QString &needle) -> bool {
        QFile f(path);
        if (!f.open(QFile::ReadOnly | QFile::Text)) {
            return false;
        }
        const QString text = QString::fromUtf8(f.readAll());
        return text.contains(needle);
    };
    const QString needle = QStringLiteral(
        "transitions intentionally dropped");

    // Branch 1 -- same-file Save (overwrite of the loaded file): the
    // breadcrumb still belongs because *this* file's transitions are
    // being dropped on disk.
    QVERIFY(state.save(src, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY2(fileContains(src, needle),
             "Same-file save must still emit the transitions breadcrumb "
             "(this is the file whose transitions data was dropped).");

    // Branch 2 -- Save-As to a NEW filename. The destination is a fresh
    // derived document; emitting the "intentionally dropped" comment
    // there would mislead a future reader into thinking the derived
    // file once had transitions of its own. Must NOT contain the line.
    const QString derived = tmpDir.path() + QDir::separator()
                            + QStringLiteral("derived.lvks");
    QVERIFY(state.save(derived, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY2(!fileContains(derived, needle),
             "Save-As to a new filename must NOT emit the transitions "
             "breadcrumb -- derived file never carried a transitions "
             "block of its own.");

    // The derived file is, by construction, a legitimate brand-new
    // .lvks; reloading it must succeed with zero rejected records and
    // an unset _loadedTransitions (no transitions block to find).
    SpriteState reloaded;
    int rejected = -1;
    QVERIFY(reloaded.load(derived, &err, &rejected));
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(rejected, 0);

    // ...and saving the reloaded derived file MUST also not regrow a
    // breadcrumb (fixed point under Save-As).
    const QString derived2 = tmpDir.path() + QDir::separator()
                             + QStringLiteral("derived2.lvks");
    QVERIFY(reloaded.save(derived2, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY2(!fileContains(derived2, needle),
             "Save of a reloaded derived file must remain breadcrumb-free.");
}

void TstLvksRoundtrip::transitionsBlockBumpsRejectedCount()
{
    // J3.2: a `transitions(...)` block at load time used to qWarning() to
    // stderr only. The GUI's MainWindow::openFile_ surfaces partial-load
    // via the rejectedCount outparam (statusBar + infoDialog), so a
    // silent transitions drop appeared to the user as a clean load. The
    // fix bumps rejectedCount for the transitions block so the same
    // surfacing fires uniformly across image / frame / transitions
    // rejections, and so the CLI --export non-zero exit code (also gated
    // on rejectedCount) lights up.

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // No-transitions baseline: must NOT bump rejectedCount.
    const QString clean = tmpDir.path() + QDir::separator()
                          + QStringLiteral("clean.lvks");
    {
        QFile f(clean);
        QVERIFY(f.open(QFile::WriteOnly | QFile::Text));
        QTextStream ts(&f);
        ts << "### LvkSprite ##\n";
        ts << "LvkSprite version 0.4\n\n";
        ts << "images(\n";
        ts << "\t0,nonexistent.png,1\n";
        ts << ")\n\n";
        ts << "frames(\n";
        ts << "\t0,solo,0,0,0,16,16\n";
        ts << ")\n\n";
        ts << "animations(\n";
        ts << "\t0,a,0\n";
        ts << "\taframes(\n";
        ts << "\t\t0,0,200,0,0,0\n";
        ts << "\t)\n";
        ts << ")\n\n";
    }

    // With-transitions file: same shape, with an additional dropped block.
    const QString withTrans = tmpDir.path() + QDir::separator()
                              + QStringLiteral("with_transitions.lvks");
    {
        QFile f(withTrans);
        QVERIFY(f.open(QFile::WriteOnly | QFile::Text));
        QTextStream ts(&f);
        ts << "### LvkSprite ##\n";
        ts << "LvkSprite version 0.4\n\n";
        ts << "images(\n";
        ts << "\t0,nonexistent.png,1\n";
        ts << ")\n\n";
        ts << "frames(\n";
        ts << "\t0,solo,0,0,0,16,16\n";
        ts << ")\n\n";
        ts << "animations(\n";
        ts << "\t0,a,0\n";
        ts << "\taframes(\n";
        ts << "\t\t0,0,200,0,0,0\n";
        ts << "\t)\n";
        ts << ")\n\n";
        ts << "transitions(\n";
        ts << "\t0,0,1,100\n";
        ts << ")\n\n";
    }

    SpriteState a;
    int rejectedA = -1;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(a.load(clean, &err, &rejectedA));
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(rejectedA, 0);

    SpriteState b;
    int rejectedB = -1;
    err = SpriteState::ErrNone;
    QVERIFY(b.load(withTrans, &err, &rejectedB));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY2(rejectedB >= 1,
             qPrintable(QString("transitions block must bump rejectedCount; "
                                "got %1").arg(rejectedB)));
}

QTEST_MAIN(TstLvksRoundtrip)
#include "tst_lvks_roundtrip.moc"
