// Integration test for undo() + save() round-trip (Team H5.7).
//
// tst_statecircularbuffer.cpp exercises StateCircularBuffer in isolation,
// and tst_lvks_roundtrip.cpp exercises save+load. Neither ties the two
// together: a regression where undo() leaves the SpriteState in a state
// that disagrees with what save() emits (e.g. an undone addImage that
// still appears in the on-disk image table) would slip past both test
// files.
//
// This test loads examples/mario.lvks via SpriteState2, mutates it with
// addImage(), undo()'s the mutation, then save+load+structurally compares
// the result against the original mario.lvks. If undo + save are in sync
// the structure is identical; otherwise the diff points straight at the
// drift.

#include <QtTest/QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QString>
#include <QTemporaryDir>

#include "spritestate2.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"

#ifndef LVK_EXAMPLES_DIR
#  define LVK_EXAMPLES_DIR ""
#endif

class TestUndoSaveIntegration : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void undoAddImageSavesAndReloadsToOriginal();

private:
    QString examplePath(const QString &name) const;
    void assertImageSetsMatch(const SpriteState &a, const SpriteState &b,
                              const QString &label) const;
};

QString TestUndoSaveIntegration::examplePath(const QString &name) const {
    const QString base = QString::fromUtf8(LVK_EXAMPLES_DIR);
    return base + QDir::separator() + name;
}

void TestUndoSaveIntegration::initTestCase() {
    const QString mario = examplePath(QStringLiteral("mario.lvks"));
    QVERIFY2(QFile::exists(mario),
             qPrintable(QStringLiteral("Missing fixture: %1").arg(mario)));
}

void TestUndoSaveIntegration::assertImageSetsMatch(
    const SpriteState &a, const SpriteState &b, const QString &label) const {
    qInfo() << "Comparing image sets for:" << label;
    const auto &iA = a.images();
    const auto &iB = b.images();
    QCOMPARE(iA.size(), iB.size());
    for (auto it = iA.constBegin(); it != iA.constEnd(); ++it) {
        QVERIFY2(iB.contains(it.key()),
                 qPrintable(QStringLiteral("image id %1 missing").arg(it.key())));
        QCOMPARE(it.value().id, iB.value(it.key()).id);
        QCOMPARE(it.value().filename, iB.value(it.key()).filename);
    }
    QCOMPARE(a.frames().size(), b.frames().size());
    QCOMPARE(a.animations().size(), b.animations().size());
}

void TestUndoSaveIntegration::undoAddImageSavesAndReloadsToOriginal() {
    // SpriteState::load resolves relative image filenames against the
    // current working directory. Set CWD to examples/ for the duration of
    // this slot so mario1.png etc. resolve.
    const QString examples = QString::fromUtf8(LVK_EXAMPLES_DIR);
    const QString savedCwd = QDir::currentPath();
    // RAII restore: any QVERIFY/QCOMPARE failure below aborts the slot
    // without reaching the manual setCurrent at the end. The scope guard
    // restores CWD regardless so subsequent test slots don't inherit the
    // examples/ directory as their working dir.
    auto restoreCwd = qScopeGuard([savedCwd]() { QDir::setCurrent(savedCwd); });
    QVERIFY(QDir::setCurrent(examples));

    SpriteState2 mutated;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(mutated.load(QStringLiteral("mario.lvks"), &err),
             qPrintable(QStringLiteral("Failed to load mario.lvks: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    // Capture the pre-mutation shape for the post-undo comparison. We
    // copy into a fresh SpriteState (the base class) so subsequent
    // SpriteState2 undo-buffer state doesn't drift the reference.
    SpriteState reference;
    QVERIFY(reference.load(QStringLiteral("mario.lvks"), &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const int origImages     = mutated.images().size();
    const int origFrames     = mutated.frames().size();
    const int origAnimations = mutated.animations().size();
    QCOMPARE(origImages,     reference.images().size());
    QCOMPARE(origFrames,     reference.frames().size());
    QCOMPARE(origAnimations, reference.animations().size());

    // Stage 1: mutate. Add an image record pointing at an existing
    // mario fixture file so the loader's whitelist accepts the path on
    // the eventual reload. addImage auto-assigns an id past the current
    // max (5 in mario.lvks -> 6).
    InputImage extra(NullId, QStringLiteral("mario1.png"), 1.0);
    mutated.addImage(extra);
    QCOMPARE(mutated.images().size(), origImages + 1);
    QVERIFY(mutated.canUndo());

    // Stage 2: undo. The added image must be removed from the live state.
    QVERIFY(mutated.undo());
    QCOMPARE(mutated.images().size(), origImages);
    QCOMPARE(mutated.frames().size(), origFrames);
    QCOMPARE(mutated.animations().size(), origAnimations);

    // Stage 3: save + reload. The on-disk file must reflect the post-undo
    // state, NOT the pre-undo state. If save() pulled from a stale
    // snapshot (e.g. one tracked alongside the undo buffer rather than
    // the live _images QHash) the reload below would carry the extra
    // image and the QCOMPARE on counts would fail.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString out = tmpDir.path() + QDir::separator()
                        + QStringLiteral("mario.undo.lvks");
    err = SpriteState::ErrNone;
    QVERIFY2(mutated.save(out, &err),
             qPrintable(QStringLiteral("save() failed after undo: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    SpriteState2 reloaded;
    err = SpriteState::ErrNone;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    // Stage 4: structural equality with the original mario.lvks.
    assertImageSetsMatch(reference, reloaded,
                         QStringLiteral("undo->save->reload vs original"));
}

QTEST_MAIN(TestUndoSaveIntegration)
#include "tst_undo_save_integration.moc"
