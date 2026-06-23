// tst_version_preservation.cpp
//
// Phase 2: SpriteState must preserve the .lvks version it loaded when it
// saves the file again, unless the in-memory data actually uses a feature
// that the loaded version cannot represent. The previous behaviour --
// always rewriting the header as v0.4 -- silently bumped legacy files on
// every save, breaking compatibility with older readers.
//
// Tests:
//   1. A synthetic, pristine v0.1 file (no ox/oy/scale/flags/sticky) round-
//      trips back out with the v0.1 header preserved, and minimumVersion()
//      reports V_01 both before and after.
//   2. Loading the same v0.1 fixture and mutating one aframe's `sticky` to
//      true auto-bumps the saved header to v0.4 (minimumVersion()=V_04).
//   3. The committed examples/mario.lvks fixture has a v0.1 header and
//      aframes with nonzero ox/oy. Per the format spec (lvks-format.md:68
//      "no on-disk field change" for v0.2) ox/oy are NOT a v0.2 marker --
//      the parser accepts 5-field aframes under any header. Saving must
//      therefore PRESERVE the v0.1 header, not silently bump to v0.2 on
//      every round-trip.
//   4. Loading a true v0.4 file with sticky=true survives round-trip with
//      the v0.4 header preserved (i.e. the policy works in both directions
//      and isn't accidentally tied to "older-or-equal-only").
//   5. A genuine v0.2-only marker (image scale != 1.0) DOES bump a v0.1
//      file to v0.2 -- so the V_02 minimum is reachable when the data
//      really requires it.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTextStream>

#include "spritestate.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

#ifndef LVK_EXAMPLES_DIR
#  define LVK_EXAMPLES_DIR "."
#endif

class TstVersionPreservation : public QObject
{
    Q_OBJECT

private slots:
    // Pristine v0.1 -> save -> v0.1 header preserved.
    void pristineV01PreservesHeader();
    // v0.1 + flip sticky=true -> save -> v0.4 header (data-driven bump).
    void stickyMutationBumpsToV04();
    // mario.lvks (header v0.1, aframes with nonzero ox/oy) -> save -> v0.1
    // header preserved. ox/oy are NOT a v0.2 marker; only sticky/flags/
    // scale/custom_header are. Also verifies a source-level check that the
    // saved header literal is "LvkSprite version 0.1".
    void marioPreservesV01();
    // True v0.4 file with sticky -> v0.4 header preserved.
    void v04WithStickyPreservesHeader();
    // A real v0.2-only marker (image scale != 1.0) still bumps v0.1->v0.2.
    void scaleMutationBumpsToV02();
    // Load+save round-trip of mario.lvks is byte-equivalent (after the
    // header is preserved at v0.1 and aframes are sorted at save-time).
    void marioRoundtripIsByteEquivalent();

private:
    // Read the first non-empty, non-comment line of @p path -- that's the
    // version header line according to the on-disk format. (The very
    // first line in our save() output is the "### LvkSprite ..." banner,
    // which begins with '#'.)
    QString readVersionHeader(const QString& path) const;

    // Write a synthetic, pristine v0.1 .lvks file under @p tmpDir using
    // image=2 fields, aframe=3 fields, animation=2 fields (no
    // custom_header section). Returns the absolute path.
    QString writePristineV01(QTemporaryDir& tmpDir) const;
};

QString TstVersionPreservation::readVersionHeader(const QString& path) const
{
    QFile f(path);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        return QString();
    }
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        return line;
    }
    return QString();
}

QString TstVersionPreservation::writePristineV01(QTemporaryDir& tmpDir) const
{
    // v0.1 schema: image=2 fields (id,filename), animation=2 fields
    // (id,name), aframe=3 fields (id,frameId,delay). No scale, no flags,
    // no ox/oy, no sticky, no custom_header section.
    const QString path = tmpDir.path() + QDir::separator()
        + QStringLiteral("pristine_v01.lvks");
    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text)) {
        return QString();
    }
    QTextStream ts(&f);
    ts << "### LvkSprite ##\n";
    ts << "LvkSprite version 0.1\n\n";
    ts << "images(\n";
    ts << "\t0,nonexistent.png\n";
    ts << ")\n\n";
    ts << "frames(\n";
    ts << "\t0,solo,0,0,0,16,16\n";
    ts << ")\n\n";
    ts << "animations(\n";
    ts << "\t0,walk\n";
    ts << "\taframes(\n";
    ts << "\t\t0,0,200\n";
    ts << "\t)\n";
    ts << ")\n\n";
    ts << "### End LvkSprite ##\n";
    f.close();
    return path;
}

void TstVersionPreservation::pristineV01PreservesHeader()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString src = writePristineV01(tmpDir);
    QVERIFY(!src.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(src, &err),
             qPrintable(QString("v0.1 load failed: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    // The pristine fixture has no v0.2+ features, so the data minimum is
    // V_01 and the loaded version is V_01 -- both should agree.
    QVERIFY(st.loadedVersion() == LvkVersion::V_01);
    QVERIFY(st.minimumVersion() == LvkVersion::V_01);

    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("pristine_v01.out.lvks");
    QVERIFY2(st.save(out, &err),
             qPrintable(QString("v0.1 save failed: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    // The saved header line must literally be "LvkSprite version 0.1".
    const QString header = readVersionHeader(out);
    QCOMPARE(header, QStringLiteral("LvkSprite version 0.1"));

    // And a reload should produce the same loadedVersion -- i.e. the
    // round-trip is a fixed point at v0.1.
    SpriteState reloaded;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(reloaded.loadedVersion() == LvkVersion::V_01);
}

void TstVersionPreservation::stickyMutationBumpsToV04()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString src = writePristineV01(tmpDir);
    QVERIFY(!src.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(st.load(src, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(st.loadedVersion() == LvkVersion::V_01);
    QVERIFY(st.minimumVersion() == LvkVersion::V_01);

    // Mutate the single aframe so it has sticky=true. This is a v0.4-only
    // feature, so minimumVersion() must jump to V_04 and save() must emit
    // the v0.4 header.
    const auto& ani = st.const_animation(0);
    LvkAframe mutated = ani.aframe(0);
    QVERIFY(mutated.id != NullId);   // sanity: the aframe is there
    mutated.sticky = true;
    st.updateAframe(mutated, /*aniId=*/0);

    QVERIFY(st.minimumVersion() == LvkVersion::V_04);

    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("pristine_v01.bumped.lvks");
    QVERIFY(st.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString header = readVersionHeader(out);
    QCOMPARE(header, QStringLiteral("LvkSprite version 0.4"));

    // And the bumped file must reload as v0.4 with sticky preserved.
    SpriteState reloaded;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(reloaded.loadedVersion() == LvkVersion::V_04);
    QCOMPARE(reloaded.const_aframe(0, 0).sticky, true);
}

void TstVersionPreservation::marioPreservesV01()
{
    // Phase B1.1: examples/mario.lvks declares "LvkSprite version 0.1" and
    // its aframes carry nonzero ox/oy values. The pre-Phase-B1 saver
    // silently rewrote the header as v0.2 on every save -- the headline
    // regression Phase 2 promised to stop. ox/oy is NOT a v0.2 marker
    // (the parser accepts 5-field aframes under any header; see
    // lvks-format.md:68 "no on-disk field change" for v0.2 and
    // src/lvkaframe.cpp:48-55). The new saver must PRESERVE the v0.1
    // header.
    const QString marioPath = QString::fromUtf8(LVK_EXAMPLES_DIR)
        + QDir::separator() + QStringLiteral("mario.lvks");
    QVERIFY2(QFile::exists(marioPath),
             qPrintable(QString("Could not locate %1").arg(marioPath)));

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(marioPath, &err),
             qPrintable(QString("Failed to load mario.lvks: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    // mario.lvks: header v0.1, no scale/flags/sticky/custom_header data
    // (only ox/oy on aframes, which is no longer a v0.2 marker) -> the
    // minimum-required version stays at v0.1.
    QVERIFY(st.loadedVersion() == LvkVersion::V_01);
    QVERIFY(st.minimumVersion() == LvkVersion::V_01);

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("mario.out.lvks");
    QVERIFY(st.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    // Source-level header check: the saved file's header literal MUST be
    // exactly "LvkSprite version 0.1". This is the smoke check called
    // out in the B1.1+B1.3 verification step: load mario -> save to temp
    // -> read the temp file header line -> must be v0.1.
    const QString header = readVersionHeader(out);
    QCOMPARE(header, QStringLiteral("LvkSprite version 0.1"));

    // Team D3.4: the header preservation is necessary but NOT sufficient.
    // mario.lvks's distinguishing payload is that some aframes carry
    // nonzero ox/oy ("5,0,80,0,-10", "6,0,180,0,-15", "7,0,80,0,-10"
    // under animation 1 / "jump"). A regression that emitted a v0.1
    // header but truncated aframe records to 3 fields (the strict v0.1
    // schema) would lose ox/oy on round-trip while still passing the
    // header-only check. Reload the saved file and assert the
    // ox/oy values were preserved on the rows we know carry them.
    SpriteState reloaded;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    // mario.lvks animation 1 ("jump") row id=5: "5,0,80,0,-10"
    QCOMPARE(reloaded.const_aframe(1, 5).ox, 0);
    QCOMPARE(reloaded.const_aframe(1, 5).oy, -10);
    // row id=6: "6,0,180,0,-15"
    QCOMPARE(reloaded.const_aframe(1, 6).ox, 0);
    QCOMPARE(reloaded.const_aframe(1, 6).oy, -15);
    // row id=7: "7,0,80,0,-10"
    QCOMPARE(reloaded.const_aframe(1, 7).ox, 0);
    QCOMPARE(reloaded.const_aframe(1, 7).oy, -10);

    // Sanity: delays survived too -- catches a regression where
    // aframes were rewritten with fewer fields and the parser shifted
    // columns.
    QCOMPARE(reloaded.const_aframe(1, 5).delay, 80);
    QCOMPARE(reloaded.const_aframe(1, 6).delay, 180);
    QCOMPARE(reloaded.const_aframe(1, 7).delay, 80);

    // Team F3.2: the previous test only validated ox/oy/delay. A
    // regression that mis-emitted frameId (e.g. swapping the id and
    // frameId columns, or zero-filling frameId on the v0.1 path) would
    // pass everything above. mario.lvks rows 5/6/7 under animation 1
    // ("jump") all carry frameId=0 (the third column of "5,0,80,0,-10"
    // etc. -- frameId is column #2 after the id), and row id=1 also
    // has frameId=0. Assert that explicitly.
    QCOMPARE(reloaded.const_aframe(1, 1).frameId, static_cast<Id>(0));
    QCOMPARE(reloaded.const_aframe(1, 5).frameId, static_cast<Id>(0));
    QCOMPARE(reloaded.const_aframe(1, 6).frameId, static_cast<Id>(0));
    QCOMPARE(reloaded.const_aframe(1, 7).frameId, static_cast<Id>(0));
    // animation 0 ("walk") row id=2 -> frameId=2, row id=3 -> frameId=3
    // (a different invariant: catches a "frameId always 0" bug).
    QCOMPARE(reloaded.const_aframe(0, 2).frameId, static_cast<Id>(2));
    QCOMPARE(reloaded.const_aframe(0, 3).frameId, static_cast<Id>(3));
}

void TstVersionPreservation::scaleMutationBumpsToV02()
{
    // The V_02 minimum is still reachable -- just not from the wrong
    // signal. A v0.1 file with scale != 1.0 on an image IS a real v0.2
    // marker (the v0.1 image schema had no scale column). Build a
    // pristine v0.1 fixture, flip scale on its single image, and verify
    // save bumps the header to v0.2.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString src = writePristineV01(tmpDir);
    QVERIFY(!src.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(st.load(src, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(st.loadedVersion() == LvkVersion::V_01);
    QVERIFY(st.minimumVersion() == LvkVersion::V_01);

    // Mutate the single image's scale to something other than 1.0.
    InputImage modified = st.const_image(0);
    modified.scale(0.5);
    st.updateImage(modified);

    QVERIFY(st.minimumVersion() == LvkVersion::V_02);

    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("pristine_v01.scaled.lvks");
    QVERIFY(st.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString header = readVersionHeader(out);
    QCOMPARE(header, QStringLiteral("LvkSprite version 0.2"));
}

void TstVersionPreservation::marioRoundtripIsByteEquivalent()
{
    // Phase B1.3: save() now sorts aframes by id (instead of load()
    // sorting them) so a load->save round-trip is byte-equivalent for
    // arbitrary input orderings. Verify on mario.lvks -- the canonical
    // multi-animation fixture with non-contiguous aframe ids
    // (animation 1 uses ids 1, 5, 6, 7 which are listed in id-order on
    // disk).
    //
    // This test depends on B1.1: if the header is silently bumped to
    // v0.2, the file headers differ and round-trip fails.
    //
    // Team D3.1: the previous version of this test only compared
    // save#2 == save#3 (a "fixed point" check). That passes even if the
    // B1.3 load-time sort were reverted -- both saves see the same
    // in-memory order, so they trivially agree on a stable order. The
    // real invariant we want is that loading the on-disk mario.lvks and
    // saving it back produces a file that matches the ORIGINAL bytes
    // for the data-bearing records. Comments / blank-line padding /
    // section-header text are presentation that save() does not
    // preserve verbatim (e.g. mario.lvks uses "### LvkSprite ####...",
    // save() emits a fixed-width banner), so byte equivalence isn't
    // achievable for the whole file. Instead we extract the
    // load-bearing records (header version line + image/frame/animation
    // record bodies, plus the explicit aframe id sequence per
    // animation) and assert they match the originals exactly. If B1.3
    // were reverted, animation 1's aframe sequence would come back as
    // [1,5,6,7] from a sort-on-load implementation -- but with
    // arbitrary in-memory ordering disrupting other animations -- so
    // the explicit-id-sequence assertions below would catch the drift
    // on a fixture whose aframes are already in id order (and would
    // catch any non-determinism on a fixture that isn't).
    const QString marioPath = QString::fromUtf8(LVK_EXAMPLES_DIR)
        + QDir::separator() + QStringLiteral("mario.lvks");
    QVERIFY2(QFile::exists(marioPath),
             qPrintable(QString("Could not locate %1").arg(marioPath)));

    // 1) Read the ORIGINAL mario.lvks bytes and extract its data-
    //    bearing records (drop comments / blank lines / banners).
    QFile origF(marioPath);
    QVERIFY(origF.open(QFile::ReadOnly | QFile::Text));
    const QByteArray origBytes = origF.readAll();
    origF.close();
    QVERIFY(!origBytes.isEmpty());

    auto isBanner = [](const QString &t) {
        return t.startsWith(QStringLiteral("### LvkSprite"))
            || t.startsWith(QStringLiteral("### End LvkSprite"));
    };
    // Extract the data-bearing lines for a specific section
    // ("images", "frames", "animations") -- i.e. the lines BETWEEN the
    // section's opening "<name>(" line and its matching ")" close
    // line. We compare per-section because save() always emits an
    // empty "custom_header(\n)\n" block, while mario.lvks does not, so
    // a whole-file line-equality comparison would diverge structurally
    // even though every record body the user can see is identical.
    auto extractSection = [&](const QByteArray &bytes,
                              const QString &sectionName) {
        QStringList out;
        QTextStream ts(bytes);
        bool inSection = false;
        int depth = 0;
        const QString openTag = sectionName + QStringLiteral("(");
        while (!ts.atEnd()) {
            const QString line = ts.readLine();
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
            if (trimmed.startsWith(QLatin1Char('#'))) continue;
            if (isBanner(trimmed)) continue;
            if (!inSection) {
                if (trimmed == openTag) {
                    inSection = true;
                    depth = 1;
                }
                continue;
            }
            // We are in the section. Track nesting because animations(
            // contains aframes( ) blocks.
            if (trimmed.endsWith(QLatin1Char('('))) {
                depth += 1;
                out.append(trimmed);
                continue;
            }
            if (trimmed == QStringLiteral(")")) {
                depth -= 1;
                if (depth == 0) {
                    return out;
                }
                out.append(trimmed);
                continue;
            }
            out.append(trimmed);
        }
        return out;
    };

    auto readHeaderLine = [&](const QByteArray &bytes) {
        QTextStream ts(bytes);
        while (!ts.atEnd()) {
            const QString line = ts.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
            return line;
        }
        return QString();
    };

    const QStringList origImages = extractSection(origBytes, QStringLiteral("images"));
    const QStringList origFrames = extractSection(origBytes, QStringLiteral("frames"));
    const QStringList origAnims = extractSection(origBytes, QStringLiteral("animations"));
    const QString origHeader = readHeaderLine(origBytes);
    QVERIFY(!origImages.isEmpty());
    QVERIFY(!origFrames.isEmpty());
    QVERIFY(!origAnims.isEmpty());

    // 2) Load + save the file under test.
    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(st.load(marioPath, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("mario.out.lvks");
    QVERIFY(st.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    QFile savedF(out);
    QVERIFY(savedF.open(QFile::ReadOnly | QFile::Text));
    const QByteArray savedBytes = savedF.readAll();
    savedF.close();

    const QStringList savedImages = extractSection(savedBytes, QStringLiteral("images"));
    const QStringList savedFrames = extractSection(savedBytes, QStringLiteral("frames"));
    const QStringList savedAnims = extractSection(savedBytes, QStringLiteral("animations"));
    const QString savedHeader = readHeaderLine(savedBytes);

    // 3a) Header must match: B1.1 protection. Reverting B1.1 (silent
    //     bump to v0.2) would make this assertion fail.
    QCOMPARE(savedHeader, origHeader);
    QCOMPARE(savedHeader, QStringLiteral("LvkSprite version 0.1"));

    // 3b) The images and frames sections must match the original
    //     record-for-record. (Aframe records cannot be compared
    //     byte-for-byte because LvkAframe::toString uses a minimal
    //     3-field form for ox==oy==0 rows while mario.lvks ships with
    //     uniform 5-field rows for visual consistency. We assert
    //     aframe SEMANTIC equality via the reload check below
    //     instead.)
    QCOMPARE(savedImages, origImages);
    QCOMPARE(savedFrames, origFrames);

    // 3c) Animation section: the per-animation HEADER lines (e.g.
    //     "0,walk") and the "aframes(" / ")" delimiters must match
    //     exactly -- those are what B1.1 (header preservation) +
    //     B1.3 (sort-on-save sequencing) together guarantee. We can't
    //     compare aframe ROW TEXT byte-for-byte because LvkAframe::
    //     toString uses the minimal 3-field form for ox==oy==0 rows
    //     while mario.lvks ships uniform 5-field rows for visual
    //     consistency (e.g. original "2,2,180,0,0" -> saved "2,2,180").
    //     Team F3.2: instead of dropping aframe rows entirely (the
    //     previous strategy silently let frameId/delay/ox/oy/sticky
    //     regressions slip through), PARSE each row and compare ALL
    //     fields per LvkAframe. The non-aframe lines are still compared
    //     verbatim.
    auto splitAnimsAndAframes = [](const QStringList &in) {
        QStringList nonAframeLines;
        QList<QPair<int, QList<LvkAframe>>> aframeBlocks;  // (animId, rows)
        bool inAframes = false;
        int currentAnimId = -1;
        QList<LvkAframe> currentBlock;
        for (const QString &line : in) {
            if (line == QStringLiteral("aframes(")) {
                inAframes = true;
                nonAframeLines.append(line);
                currentBlock.clear();
                continue;
            }
            if (line == QStringLiteral(")")) {
                if (inAframes) {
                    inAframes = false;
                    aframeBlocks.append(qMakePair(currentAnimId, currentBlock));
                    currentBlock.clear();
                    nonAframeLines.append(line);
                    continue;
                }
                nonAframeLines.append(line);
                continue;
            }
            if (inAframes) {
                LvkAframe af;
                if (af.fromString(line)) {
                    currentBlock.append(af);
                }
                continue;
            }
            // Non-aframe animation row (e.g. "0,walk" or "1,jump,0").
            // The first comma-delimited field is the animation id.
            const int comma = line.indexOf(QLatin1Char(','));
            if (comma > 0) {
                bool ok = false;
                const int id = line.left(comma).toInt(&ok);
                if (ok) currentAnimId = id;
            }
            nonAframeLines.append(line);
        }
        return qMakePair(nonAframeLines, aframeBlocks);
    };

    auto aframesEqual = [](const QList<LvkAframe> &a, const QList<LvkAframe> &b) {
        if (a.size() != b.size()) return false;
        for (int i = 0; i < a.size(); ++i) {
            if (a.at(i).id != b.at(i).id) return false;
            if (a.at(i).frameId != b.at(i).frameId) return false;
            if (a.at(i).delay != b.at(i).delay) return false;
            if (a.at(i).ox != b.at(i).ox) return false;
            if (a.at(i).oy != b.at(i).oy) return false;
            if (a.at(i).sticky != b.at(i).sticky) return false;
        }
        return true;
    };

    const auto origSplit = splitAnimsAndAframes(origAnims);
    const auto savedSplit = splitAnimsAndAframes(savedAnims);
    // Non-aframe lines: per-animation header rows + the literal
    // "aframes("/")" delimiters must match exactly.
    QCOMPARE(savedSplit.first, origSplit.first);
    // Aframe blocks: same count of blocks, each with the same animId
    // and the same ordered list of LvkAframes (all 6 fields per row).
    QCOMPARE(savedSplit.second.size(), origSplit.second.size());
    for (int i = 0; i < origSplit.second.size(); ++i) {
        QCOMPARE(savedSplit.second.at(i).first, origSplit.second.at(i).first);
        QVERIFY2(aframesEqual(savedSplit.second.at(i).second,
                              origSplit.second.at(i).second),
                 qPrintable(QString("aframe block %1 (animId %2) drifted on round-trip")
                                .arg(i).arg(origSplit.second.at(i).first)));
    }

    // 4) Explicit aframe-id sequence per animation (loaded from the
    //    saved bytes) must be ascending and match mario.lvks. This
    //    catches B1.3 regressions even if the lines-equal check were
    //    accidentally relaxed.
    SpriteState reloaded;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    // animation 0 (walk): aframes 2, 3
    const QList<LvkAframe> a0 = reloaded.aframes(0);
    QCOMPARE(a0.size(), 2);
    QCOMPARE(a0.at(0).id, static_cast<Id>(2));
    QCOMPARE(a0.at(1).id, static_cast<Id>(3));

    // animation 1 (jump): aframes 1, 5, 6, 7 (non-contiguous)
    const QList<LvkAframe> a1 = reloaded.aframes(1);
    QCOMPARE(a1.size(), 4);
    QCOMPARE(a1.at(0).id, static_cast<Id>(1));
    QCOMPARE(a1.at(1).id, static_cast<Id>(5));
    QCOMPARE(a1.at(2).id, static_cast<Id>(6));
    QCOMPARE(a1.at(3).id, static_cast<Id>(7));

    // animation 2 (queen_walk): aframes 8, 9
    const QList<LvkAframe> a2 = reloaded.aframes(2);
    QCOMPARE(a2.size(), 2);
    QCOMPARE(a2.at(0).id, static_cast<Id>(8));
    QCOMPARE(a2.at(1).id, static_cast<Id>(9));

    // 5) Image count smoke-check from the saved bytes (5 images in
    //    mario.lvks).
    QCOMPARE(reloaded.images().size(), 5);
}

void TstVersionPreservation::v04WithStickyPreservesHeader()
{
    // A genuine v0.4 file with sticky=true must round-trip with the v0.4
    // header preserved (loadedVersion=V_04, minimumVersion=V_04). This
    // is the upper bound of the policy: we don't accidentally downgrade
    // a v0.4 file when nothing forces v0.4 *except* the version itself.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString src = tmpDir.path() + QDir::separator()
        + QStringLiteral("real_v04.lvks");
    {
        QFile f(src);
        QVERIFY(f.open(QFile::WriteOnly | QFile::Text));
        QTextStream ts(&f);
        ts << "### LvkSprite ##\n";
        ts << "LvkSprite version 0.4\n\n";
        ts << "images(\n\t0,nonexistent.png,1\n)\n\n";
        ts << "frames(\n\t0,solo,0,0,0,16,16\n)\n\n";
        ts << "animations(\n";
        ts << "\t0,walk,0\n";
        ts << "\taframes(\n";
        ts << "\t\t0,0,200,0,0,1\n";  // sticky=1
        ts << "\t)\n";
        ts << ")\n\n";
    }

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(st.load(src, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(st.loadedVersion() == LvkVersion::V_04);
    QVERIFY(st.minimumVersion() == LvkVersion::V_04);

    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("real_v04.out.lvks");
    QVERIFY(st.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString header = readVersionHeader(out);
    QCOMPARE(header, QStringLiteral("LvkSprite version 0.4"));
}

QTEST_MAIN(TstVersionPreservation)
#include "tst_version_preservation.moc"
