// tst_aframe_order.cpp
//
// Phase 3 (Item 11): aframe in-memory order must follow aframe id, not
// the order they happened to appear on disk. The pre-Phase-3 code path
// used QList::append() inside SpriteState::addAframe(), which lost the
// id ordering for hand-edited / post-delete-saves where ids are not in
// monotonically increasing order. The fix is a post-load sort by id.
//
// This test writes a fixture .lvks where aframes are listed as ids
// [3, 1, 2] inside a single animation, loads it, and asserts the in-
// memory order is [1, 2, 3].

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
    void aframesSortedByIdAfterLoad();
};

void TstAframeOrder::aframesSortedByIdAfterLoad()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString path = tmpDir.path() + QDir::separator()
        + QStringLiteral("nonseq_aframes.lvks");
    {
        QFile f(path);
        QVERIFY(f.open(QFile::WriteOnly | QFile::Text));
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
    }

    SpriteState st;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(path, &err),
             qPrintable(QString("load failed: %1")
                            .arg(SpriteState::errorMessage(err))));
    QCOMPARE(err, SpriteState::ErrNone);

    const QList<LvkAframe>& aframes = st.aframes(/*aniId=*/0);
    QCOMPARE(aframes.size(), 3);

    // Post-load sort by id: [1, 2, 3] in memory regardless of file order.
    QCOMPARE(aframes.at(0).id, static_cast<Id>(1));
    QCOMPARE(aframes.at(1).id, static_cast<Id>(2));
    QCOMPARE(aframes.at(2).id, static_cast<Id>(3));

    // Sanity: each aframe's delay still maps to its original id (i.e.
    // we sorted the right records, not just the ids).
    QCOMPARE(aframes.at(0).delay, 200);  // id=1 had delay=200
    QCOMPARE(aframes.at(1).delay, 300);  // id=2 had delay=300
    QCOMPARE(aframes.at(2).delay, 100);  // id=3 had delay=100
}

QTEST_MAIN(TstAframeOrder)
#include "tst_aframe_order.moc"
