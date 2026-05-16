// tst_lvks_versions.cpp
//
// Version-header parse test. For each of the .lvks format versions
// currently supported by SpriteState::load() — v0.1, v0.2, v0.3, v0.4 —
// build a minimal in-memory fixture using the appropriate per-record
// field count and verify the parser accepts it without error.
//
// Per-version record shapes were derived from:
//   src/lvkframe.cpp:18-48      — frame: 7 fields (id,name,imgId,ox,oy,w,h)
//   src/lvkaframe.cpp:18-62     — aframe: 3 / 5 / 6 fields by version
//   src/lvkanimation.cpp:18-43  — animation: 2 / 3 fields by version
//   src/inputimage.cpp:18-49    — image: 2 / 3 fields by version
// Version header constants live at src/spritestate.cpp:16-20.
//
// Authored by Agent 2/10.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "spritestate.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

class TstLvksVersions : public QObject
{
    Q_OBJECT

private slots:
    // SpriteState::load() should accept each historical header literal.
    void parsesV01();
    void parsesV02();
    void parsesV03();
    void parsesV04();

    // The version chooser must reject a header that does not name a known
    // version — otherwise a bug in a later format upgrade could silently
    // turn the parser into a permissive nop.
    void rejectsUnknownVersion();

    // The fromString() chain in each data class is the actual
    // compatibility layer (see plan section "Format compatibility
    // strategy"). Spot-check each backward-compat branch directly so a
    // regression in those chains is caught without a full file parse.
    void aframeAcceptsLegacyFieldCounts();
    void animationAcceptsLegacyFieldCounts();
    void imageAcceptsLegacyFieldCounts();

private:
    // Build a minimal lvks file under @p dir using @p versionHeader as the
    // version line and the per-version record shapes appropriate for it.
    // Returns the absolute path of the written file.
    QString writeMinimalFixture(QTemporaryDir& dir,
                                const QString& versionHeader,
                                int aframeFields,
                                int animationFields,
                                int imageFields,
                                bool includeCustomHeader) const;
};

QString TstLvksVersions::writeMinimalFixture(QTemporaryDir& dir,
                                             const QString& versionHeader,
                                             int aframeFields,
                                             int animationFields,
                                             int imageFields,
                                             bool includeCustomHeader) const
{
    const QString filename =
        dir.path() + QDir::separator() +
        QStringLiteral("fixture_") + QString(versionHeader)
            .replace(QChar(' '), QChar('_'))
            .replace(QChar('.'), QChar('_')) +
        QStringLiteral(".lvks");

    QFile f(filename);
    if (!f.open(QFile::WriteOnly | QFile::Text)) {
        return QString();
    }

    QTextStream ts(&f);
    ts << "### LvkSprite ##############################\n";
    ts << versionHeader << "\n\n";

    // Images section.
    ts << "images(\n";
    if (imageFields == 2) {
        // v0.1 / v0.2 layout: id,filename
        ts << "\t0,nonexistent.png\n";
        ts << "\t1,other.png\n";
    } else {
        // v0.3 / v0.4 layout: id,filename,scale
        ts << "\t0,nonexistent.png,1\n";
        ts << "\t1,other.png,2\n";
    }
    ts << ")\n\n";

    // Frames section — same field count across all versions.
    ts << "frames(\n";
    ts << "\t0,frame_a,0,0,0,16,16\n";
    ts << "\t1,frame_b,1,2,3,8,12\n";
    ts << ")\n\n";

    // Animations + aframes section.
    ts << "animations(\n";
    {
        // Animation record.
        if (animationFields == 2) {
            ts << "\t0,walk\n";
        } else {
            // v0.3 / v0.4 added a flags column.
            ts << "\t0,walk,0\n";
        }
        // Aframe records under this animation.
        ts << "\taframes(\n";
        if (aframeFields == 3) {
            // Original v0.1 aframe layout.
            ts << "\t\t0,0,200\n";
            ts << "\t\t1,1,180\n";
        } else if (aframeFields == 5) {
            // v0.2 added (ox, oy).
            ts << "\t\t0,0,200,0,0\n";
            ts << "\t\t1,1,180,4,-2\n";
        } else { // 6 fields
            // v0.4 added the sticky flag.
            ts << "\t\t0,0,200,0,0,0\n";
            ts << "\t\t1,1,180,4,-2,1\n";
        }
        ts << "\t)\n";
    }
    ts << ")\n\n";

    if (includeCustomHeader) {
        ts << "custom_header(\n";
        ts << "#define MARKER 42\n";
        ts << ")\n\n";
    }

    ts << "### End LvkSprite ##########################\n";
    f.close();
    return filename;
}

// ----- per-version parse tests ----------------------------------------

void TstLvksVersions::parsesV01()
{
    // v0.1: image=2 fields, animation=2 fields, aframe=3 fields,
    // no custom_header section.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeMinimalFixture(dir,
        QStringLiteral("LvkSprite version 0.1"),
        /*aframeFields=*/3,
        /*animationFields=*/2,
        /*imageFields=*/2,
        /*includeCustomHeader=*/false);
    QVERIFY(!path.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(path, &err),
             qPrintable(QString("v0.1 parse failed: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(st.images().size(),     2);
    QCOMPARE(st.frames().size(),     2);
    QCOMPARE(st.animations().size(), 1);
    QCOMPARE(st.animations().value(0)._aframes.size(), 2);

    // v0.1 defaults: animation flags = 0, aframe ox/oy = 0, sticky = false.
    QCOMPARE(st.animations().value(0).flags, 0u);
    QCOMPARE(st.animations().value(0)._aframes.at(0).ox, 0);
    QCOMPARE(st.animations().value(0)._aframes.at(0).oy, 0);
    QCOMPARE(st.animations().value(0)._aframes.at(0).sticky, false);
}

void TstLvksVersions::parsesV02()
{
    // v0.2: image=2 fields, animation=2 fields, aframe=5 fields (ox,oy added).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeMinimalFixture(dir,
        QStringLiteral("LvkSprite version 0.2"),
        /*aframeFields=*/5,
        /*animationFields=*/2,
        /*imageFields=*/2,
        /*includeCustomHeader=*/false);
    QVERIFY(!path.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(path, &err),
             qPrintable(QString("v0.2 parse failed: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(st.images().size(),     2);
    QCOMPARE(st.frames().size(),     2);
    QCOMPARE(st.animations().size(), 1);
    QCOMPARE(st.animations().value(0)._aframes.size(), 2);

    // Verify the ox/oy were actually read.
    QCOMPARE(st.animations().value(0)._aframes.at(1).ox,  4);
    QCOMPARE(st.animations().value(0)._aframes.at(1).oy, -2);
    // sticky still defaults to false in v0.2.
    QCOMPARE(st.animations().value(0)._aframes.at(0).sticky, false);
}

void TstLvksVersions::parsesV03()
{
    // v0.3 introduced the custom_header() section and the image-scale
    // column. Animation flags column also exists in v0.3.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeMinimalFixture(dir,
        QStringLiteral("LvkSprite version 0.3"),
        /*aframeFields=*/5,
        /*animationFields=*/3,
        /*imageFields=*/3,
        /*includeCustomHeader=*/true);
    QVERIFY(!path.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(path, &err),
             qPrintable(QString("v0.3 parse failed: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(st.images().size(),     2);
    QCOMPARE(st.frames().size(),     2);
    QCOMPARE(st.animations().size(), 1);
    QVERIFY(st.getCustomHeader().contains(QStringLiteral("MARKER 42")));
    // Scale column was provided as "2" on image id 1.
    QCOMPARE(st.images().value(1).scale(), 2.0);
}

void TstLvksVersions::parsesV04()
{
    // v0.4 added the sticky flag => aframe is now 6 fields.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeMinimalFixture(dir,
        QStringLiteral("LvkSprite version 0.4"),
        /*aframeFields=*/6,
        /*animationFields=*/3,
        /*imageFields=*/3,
        /*includeCustomHeader=*/true);
    QVERIFY(!path.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(path, &err),
             qPrintable(QString("v0.4 parse failed: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(st.images().size(),     2);
    QCOMPARE(st.frames().size(),     2);
    QCOMPARE(st.animations().size(), 1);
    QCOMPARE(st.animations().value(0)._aframes.size(), 2);
    QCOMPARE(st.animations().value(0)._aframes.at(0).sticky, false);
    QCOMPARE(st.animations().value(0)._aframes.at(1).sticky, true);
}

void TstLvksVersions::rejectsUnknownVersion()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filename =
        dir.path() + QDir::separator() + QStringLiteral("bogus.lvks");
    {
        QFile f(filename);
        QVERIFY(f.open(QFile::WriteOnly | QFile::Text));
        QTextStream ts(&f);
        ts << "### LvkSprite ##\n";
        ts << "LvkSprite version 999.999\n";   // not a known version
        ts << "images(\n)\n";
    }

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(!st.load(filename, &err),
             "Parser must reject an unknown version header but accepted it");
    QCOMPARE(err, SpriteState::ErrInvalidFormat);
}

// ----- direct fromString() compat-layer spot checks --------------------

void TstLvksVersions::aframeAcceptsLegacyFieldCounts()
{
    // 3 fields — original v0.1 layout.
    LvkAframe f3;
    QVERIFY(f3.fromString(QStringLiteral("7,3,200")));
    QCOMPARE(f3.id, 7);
    QCOMPARE(f3.frameId, 3);
    QCOMPARE(f3.delay, 200);
    QCOMPARE(f3.ox, 0);
    QCOMPARE(f3.oy, 0);
    QCOMPARE(f3.sticky, false);

    // 5 fields — v0.2 layout.
    LvkAframe f5;
    QVERIFY(f5.fromString(QStringLiteral("7,3,200,4,-2")));
    QCOMPARE(f5.ox, 4);
    QCOMPARE(f5.oy, -2);
    QCOMPARE(f5.sticky, false);

    // 6 fields — v0.4 layout.
    LvkAframe f6;
    QVERIFY(f6.fromString(QStringLiteral("7,3,200,4,-2,1")));
    QCOMPARE(f6.sticky, true);

    // 4 fields is not a documented shape — must be rejected.
    LvkAframe fbad;
    QVERIFY(!fbad.fromString(QStringLiteral("7,3,200,4")));
}

void TstLvksVersions::animationAcceptsLegacyFieldCounts()
{
    // 2 fields — v0.1 layout (no flags).
    LvkAnimation a2;
    QVERIFY(a2.fromString(QStringLiteral("0,walk")));
    QCOMPARE(a2.id, 0);
    QCOMPARE(a2.name, QStringLiteral("walk"));
    QCOMPARE(a2.flags, 0u);

    // 3 fields — v0.3+ layout (flags added).
    LvkAnimation a3;
    QVERIFY(a3.fromString(QStringLiteral("4,jump,5")));
    QCOMPARE(a3.id, 4);
    QCOMPARE(a3.name, QStringLiteral("jump"));
    QCOMPARE(a3.flags, 5u);

    // 1 field is malformed.
    LvkAnimation abad;
    QVERIFY(!abad.fromString(QStringLiteral("0")));
}

void TstLvksVersions::imageAcceptsLegacyFieldCounts()
{
    // 2 fields — v0.1/v0.2 layout (no scale).
    InputImage i2;
    QVERIFY(i2.fromString(QStringLiteral("0,nonexistent.png")));
    QCOMPARE(i2.id, 0);
    QCOMPARE(i2.filename, QStringLiteral("nonexistent.png"));
    QCOMPARE(i2.scale(), 1.0);

    // 3 fields — v0.3+ layout (scale).
    InputImage i3;
    QVERIFY(i3.fromString(QStringLiteral("1,nonexistent.png,2")));
    QCOMPARE(i3.scale(), 2.0);

    // 1 field is malformed.
    InputImage ibad;
    QVERIFY(!ibad.fromString(QStringLiteral("0")));

    // 4 fields is also malformed (no version uses 4).
    InputImage ibad2;
    QVERIFY(!ibad2.fromString(QStringLiteral("0,x.png,1,extra")));
}

QTEST_MAIN(TstLvksVersions)
#include "tst_lvks_versions.moc"
