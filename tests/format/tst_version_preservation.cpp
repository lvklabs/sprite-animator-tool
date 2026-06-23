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
    const QString marioPath = QString::fromUtf8(LVK_EXAMPLES_DIR)
        + QDir::separator() + QStringLiteral("mario.lvks");
    QVERIFY2(QFile::exists(marioPath),
             qPrintable(QString("Could not locate %1").arg(marioPath)));

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

    // After the first save, every subsequent save must produce the same
    // bytes -- the canonical form is a fixed point. We don't compare
    // against the original mario.lvks bytes (it has decorative comments
    // and whitespace that save() does not preserve verbatim), only that
    // a second save matches the first. Together with the v0.1 header
    // assertion in marioPreservesV01() this guards against future
    // header-drift bugs.
    SpriteState reloaded;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString out2 = tmpDir.path() + QDir::separator()
        + QStringLiteral("mario.out2.lvks");
    QVERIFY(reloaded.save(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    QFile f1(out), f2(out2);
    QVERIFY(f1.open(QFile::ReadOnly));
    QVERIFY(f2.open(QFile::ReadOnly));
    const QByteArray b1 = f1.readAll();
    const QByteArray b2 = f2.readAll();
    QCOMPARE(b1, b2);
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
