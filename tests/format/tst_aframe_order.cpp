// tst_aframe_order.cpp
//
// Phase B1.3: aframe ordering is now normalized at SAVE time, not LOAD
// time. The pre-B1.3 code sorted aframes by id after load and left save
// to iterate the in-memory list as-is; that produced round-trip byte
// changes for any file whose on-disk aframe order was non-sequential by
// id. The new policy:
//
//   * load() preserves the file's aframe order in memory (addAframe
//     appends in file order).
//   * save() sorts aframes by id before writing them, so the on-disk
//     form is canonical and load->save is a fixed point.
//
// This test writes a fixture .lvks where aframes are listed as ids
// [3, 1, 2], loads it (in-memory order MUST match the file order), and
// then saves and re-loads -- after which the in-memory order MUST be
// [1, 2, 3] because save normalised them.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTextStream>

#include "spritestate.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

class TstAframeOrder : public QObject
{
    Q_OBJECT

private slots:
    void loadPreservesFileOrder();
    void saveNormalizesById();
    void roundtripIsByteEquivalent();
    // Team D3.5: B1.3 added a save-time sort for the .lvks path but the
    // Cocos2d .lkot exporter iterated _aframes directly, so for a non-
    // sequential input the exported .lkot file came out in the in-memory
    // (=file) order. This test loads a fixture with aframes [3, 1, 2],
    // exports to Cocos2d, and asserts the .lkot's aframe rows appear in
    // ascending id order.
    void lkotExportSortsAframesById();
};

namespace {
QString writeNonseqFixture(QTemporaryDir& tmpDir)
{
    const QString path = tmpDir.path() + QDir::separator()
        + QStringLiteral("nonseq_aframes.lvks");
    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text)) return QString();
    QTextStream ts(&f);
    // v0.1 schema is sufficient: aframe = (id, frameId, delay).
    // We intentionally list aframes in id order [3, 1, 2] to simulate
    // a hand-edited / post-delete-save file.
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
    ts << "\t\t3,0,100\n";
    ts << "\t\t1,0,200\n";
    ts << "\t\t2,0,300\n";
    ts << "\t)\n";
    ts << ")\n\n";
    ts << "### End LvkSprite ##\n";
    return path;
}
}

void TstAframeOrder::loadPreservesFileOrder()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString path = writeNonseqFixture(tmpDir);
    QVERIFY(!path.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(path, &err),
             qPrintable(QString("load failed: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    const QList<LvkAframe>& aframes = st.aframes(/*aniId=*/0);
    QCOMPARE(aframes.size(), 3);

    // load() preserves file order. The file listed [3, 1, 2].
    QCOMPARE(aframes.at(0).id, static_cast<Id>(3));
    QCOMPARE(aframes.at(1).id, static_cast<Id>(1));
    QCOMPARE(aframes.at(2).id, static_cast<Id>(2));

    // Sanity: each aframe's delay still maps to its original id.
    QCOMPARE(aframes.at(0).delay, 100);  // id=3 had delay=100
    QCOMPARE(aframes.at(1).delay, 200);  // id=1 had delay=200
    QCOMPARE(aframes.at(2).delay, 300);  // id=2 had delay=300
}

void TstAframeOrder::saveNormalizesById()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString path = writeNonseqFixture(tmpDir);
    QVERIFY(!path.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(st.load(path, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("nonseq.out.lvks");
    QVERIFY(st.save(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    // Reload the saved file. Because save normalised, the in-memory
    // order after reload must be the id-sorted [1, 2, 3].
    SpriteState reloaded;
    QVERIFY(reloaded.load(out, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QList<LvkAframe>& aframes = reloaded.aframes(/*aniId=*/0);
    QCOMPARE(aframes.size(), 3);
    QCOMPARE(aframes.at(0).id, static_cast<Id>(1));
    QCOMPARE(aframes.at(1).id, static_cast<Id>(2));
    QCOMPARE(aframes.at(2).id, static_cast<Id>(3));

    // The original mapping survives sort.
    QCOMPARE(aframes.at(0).delay, 200);  // id=1 had delay=200
    QCOMPARE(aframes.at(1).delay, 300);  // id=2 had delay=300
    QCOMPARE(aframes.at(2).delay, 100);  // id=3 had delay=100
}

void TstAframeOrder::roundtripIsByteEquivalent()
{
    // Two saves in a row must produce byte-equivalent files. That is the
    // strongest form of the "canonical fixed point" property -- any
    // remaining non-determinism (sort instability, header drift, etc.)
    // would surface here.
    //
    // Team D3.1: in addition to the save==save fixed-point property
    // (necessary but trivially passes even if the load-time sort were
    // restored), assert what the B1.3 production fix actually
    // *guarantees*: the aframes in the first save's bytes appear in
    // ascending id order, even though the source file lists them as
    // [3, 1, 2]. If save() were reverted to iterate _aframes in
    // in-memory (=file) order, the saved bytes would contain the row
    // sequence 3,1,2 instead of 1,2,3 and the assertion below would
    // fail.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString path = writeNonseqFixture(tmpDir);
    QVERIFY(!path.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(st.load(path, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString out1 = tmpDir.path() + QDir::separator()
        + QStringLiteral("nonseq.out1.lvks");
    QVERIFY(st.save(out1, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    SpriteState reloaded;
    QVERIFY(reloaded.load(out1, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString out2 = tmpDir.path() + QDir::separator()
        + QStringLiteral("nonseq.out2.lvks");
    QVERIFY(reloaded.save(out2, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    QFile f1(out1), f2(out2);
    QVERIFY(f1.open(QFile::ReadOnly));
    QVERIFY(f2.open(QFile::ReadOnly));
    const QByteArray bytes1 = f1.readAll();
    const QByteArray bytes2 = f2.readAll();
    QCOMPARE(bytes1, bytes2);

    // Stronger: the first save's bytes must list the aframes in
    // ASCENDING id order, NOT the [3, 1, 2] file order they were
    // loaded in. This is the property B1.3 actually establishes; the
    // save==save fixed-point check above passes even if B1.3 were
    // reverted (so long as save is deterministic).
    QStringList aframeRows;
    {
        QTextStream ts(bytes1);
        bool inAframes = false;
        while (!ts.atEnd()) {
            const QString line = ts.readLine();
            const QString trimmed = line.trimmed();
            if (trimmed == QStringLiteral("aframes(")) {
                inAframes = true;
                continue;
            }
            if (inAframes && trimmed == QStringLiteral(")")) {
                inAframes = false;
                continue;
            }
            if (inAframes && !trimmed.isEmpty() &&
                !trimmed.startsWith(QLatin1Char('#'))) {
                aframeRows.append(trimmed);
            }
        }
    }
    QCOMPARE(aframeRows.size(), 3);
    // Row format: "id,frameId,delay" -- the leading column is the id.
    QVERIFY(aframeRows.at(0).startsWith(QStringLiteral("1,")));
    QVERIFY(aframeRows.at(1).startsWith(QStringLiteral("2,")));
    QVERIFY(aframeRows.at(2).startsWith(QStringLiteral("3,")));
}

void TstAframeOrder::lkotExportSortsAframesById()
{
    // Team D3.5: load the [3, 1, 2] fixture and export it to the
    // Cocos2d .lkot/.lkob/.h trio. The exporter is supposed to write
    // aframes in ascending id order so .lkot stays byte-equivalent
    // across runs with arbitrary input orderings. Before the D3.5 fix,
    // the .lkot rows came out as the in-memory [3, 1, 2] sequence.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString path = writeNonseqFixture(tmpDir);
    QVERIFY(!path.isEmpty());

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(st.load(path, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    // exportSprite emits <basename>.lkot etc into outputDir. The
    // baseName is taken from the .lvks filename, so picking a fresh
    // name ("nsq_export.lvks") avoids any collision with the source.
    const QString outDir = tmpDir.path();
    const QString exportName = outDir + QDir::separator()
        + QStringLiteral("nsq_export.lvks");
    QVERIFY(st.exportSprite(exportName, outDir, QString(),
                            SpriteState::Cocos2d, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString lkot = outDir + QDir::separator()
        + QStringLiteral("nsq_export.lkot");
    QVERIFY(QFile::exists(lkot));

    // Parse the .lkot looking for aframes(...) -> ) block and pull out
    // the leading id from each row.
    QFile f(lkot);
    QVERIFY(f.open(QFile::ReadOnly | QFile::Text));
    const QString content = QString::fromUtf8(f.readAll());
    f.close();

    QStringList rowIds;
    bool inAframes = false;
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed == QStringLiteral("aframes(")) {
            inAframes = true;
            continue;
        }
        if (inAframes && trimmed == QStringLiteral(")")) {
            inAframes = false;
            continue;
        }
        if (inAframes && !trimmed.isEmpty() &&
            !trimmed.startsWith(QLatin1Char('#'))) {
            // "id,frameId,delay,..." -- grab everything up to first ','.
            const int comma = trimmed.indexOf(QLatin1Char(','));
            if (comma > 0) {
                rowIds.append(trimmed.left(comma));
            }
        }
    }

    QCOMPARE(rowIds.size(), 3);
    QCOMPARE(rowIds.at(0), QStringLiteral("1"));
    QCOMPARE(rowIds.at(1), QStringLiteral("2"));
    QCOMPARE(rowIds.at(2), QStringLiteral("3"));
}

QTEST_MAIN(TstAframeOrder)
#include "tst_aframe_order.moc"
