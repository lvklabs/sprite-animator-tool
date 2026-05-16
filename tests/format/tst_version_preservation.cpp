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
//   3. The committed examples/mario.lvks fixture has a v0.1 header but its
//      aframes carry nonzero ox/oy (v0.2 features). Saving must therefore
//      bump to v0.2 -- not silently bury the data, not silently downgrade,
//      not jump straight to v0.4.
//   4. Loading a true v0.4 file with sticky=true survives round-trip with
//      the v0.4 header preserved (i.e. the policy works in both directions
//      and isn't accidentally tied to "older-or-equal-only").

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
    // mario.lvks has a v0.1 header but v0.2 data; save must bump to v0.2.
    void marioBumpsToV02();
    // True v0.4 file with sticky -> v0.4 header preserved.
    void v04WithStickyPreservesHeader();

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

void TstVersionPreservation::marioBumpsToV02()
{
    // examples/mario.lvks declares "LvkSprite version 0.1" but its
    // aframes carry nonzero ox/oy values (a v0.2 column). The old saver
    // silently rewrote the header as v0.4; the new saver must bump to
    // exactly v0.2 -- the smallest version that can hold the data --
    // not all the way to v0.4.
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

    // mario.lvks: header v0.1, but data has nonzero ox/oy -> minimum v0.2.
    QVERIFY(st.loadedVersion() == LvkVersion::V_01);
    QVERIFY(st.minimumVersion() == LvkVersion::V_02);

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("mario.out.lvks");
    QVERIFY(st.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString header = readVersionHeader(out);
    QCOMPARE(header, QStringLiteral("LvkSprite version 0.2"));
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
