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
    QCOMPARE(f1.readAll(), f2.readAll());
}

QTEST_MAIN(TstAframeOrder)
#include "tst_aframe_order.moc"
