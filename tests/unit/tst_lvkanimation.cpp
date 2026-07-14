// Unit tests for LvkAnimation::toString / fromString and its aframe list
// management.
//
// Serialized format:
//   3 fields (v0.3+):   id, name, flags
//   2 fields (legacy):  id, name              -> flags defaults to 0

#include <QtTest/QtTest>

#include "lvkanimation.h"
#include "lvkaframe.h"

class TestLvkAnimation : public QObject
{
    Q_OBJECT

private slots:
    void testToStringThreeFields();
    void testFromStringThreeFields();
    void testFromStringTwoFieldsLegacyDefaultsFlags();
    void testRoundTripFromToString();
    void testFromStringRejectsArityOne();
    void testFromStringRejectsArityFour();
    void testAddAframeAndLookup();
    void testRemoveAframe();
    void testRemoveAframeRemovesSingleMatchOnly();
    void testSwapAframes();
    void testEqualityOperator();
};

void TestLvkAnimation::testToStringThreeFields()
{
    LvkAnimation a(7, "walk", 0x3);
    const QString s = a.toString();
    QCOMPARE(s.count(','), 2);
    QCOMPARE(s, QString("7,walk,3"));
}

void TestLvkAnimation::testFromStringThreeFields()
{
    LvkAnimation a;
    QVERIFY(a.fromString("11,run,5"));
    QCOMPARE(a.id, 11);
    QCOMPARE(a.name, QString("run"));
    QCOMPARE(a.flags, 5u);
}

void TestLvkAnimation::testFromStringTwoFieldsLegacyDefaultsFlags()
{
    LvkAnimation a;
    QVERIFY(a.fromString("4,idle"));
    QCOMPARE(a.id, 4);
    QCOMPARE(a.name, QString("idle"));
    QCOMPARE(a.flags, 0u);
}

void TestLvkAnimation::testRoundTripFromToString()
{
    LvkAnimation original(2, "jump", 0xCAFE);
    LvkAnimation parsed;
    QVERIFY(parsed.fromString(original.toString()));
    // operator== compares aframes too; populate identically (both empty).
    QVERIFY(parsed == original);
}

void TestLvkAnimation::testFromStringRejectsArityOne()
{
    LvkAnimation a;
    QVERIFY(!a.fromString("just-an-id"));
}

void TestLvkAnimation::testFromStringRejectsArityFour()
{
    LvkAnimation a;
    QVERIFY(!a.fromString("1,name,2,3"));
}

void TestLvkAnimation::testAddAframeAndLookup()
{
    LvkAnimation ani(1, "spin", 0);
    LvkAframe af0(0, 100, 200);
    LvkAframe af1(1, 101, 250);
    ani.addAframe(af0);
    ani.addAframe(af1);
    QCOMPARE(ani._aframes.size(), 2);
    QCOMPARE(ani.aframe(0).frameId, 100);
    QCOMPARE(ani.aframe(1).frameId, 101);
}

void TestLvkAnimation::testRemoveAframe()
{
    LvkAnimation ani(1, "spin", 0);
    ani.addAframe(LvkAframe(0, 100, 200));
    ani.addAframe(LvkAframe(1, 101, 250));
    ani.addAframe(LvkAframe(2, 102, 300));
    ani.removeAframe(1);
    QCOMPARE(ani._aframes.size(), 2);
    // remaining ids should be 0 and 2
    QCOMPARE(ani._aframes.at(0).id, 0);
    QCOMPARE(ani._aframes.at(1).id, 2);
}

void TestLvkAnimation::testRemoveAframeRemovesSingleMatchOnly()
{
    // The API name removeAframe (singular) documents single-removal
    // semantics. Pre-fix the loop kept iterating after removeAt() and the
    // post-increment skipped the now-shifted element -- meaning duplicate
    // ids would have one match removed and a second silently skipped.
    // Deliberately violate id-uniqueness here (production code prevents
    // duplicates, but the helper itself must behave per its contract).
    LvkAnimation ani(1, "spin", 0);
    LvkAframe dup1, dup2, other;
    dup1.id = 5;
    dup1.frameId = 100;
    dup2.id = 5;
    dup2.frameId = 101;
    other.id = 9;
    other.frameId = 200;
    ani.addAframe(dup1);
    ani.addAframe(dup2);
    ani.addAframe(other);
    QCOMPARE(ani._aframes.size(), 3);

    ani.removeAframe(5);

    // Exactly one of the two id=5 entries should remain; total size drops
    // by one (not two).
    QCOMPARE(ani._aframes.size(), 2);
    // The unrelated id=9 aframe must still be present.
    QCOMPARE(ani.aframe(9).frameId, 200);
}

void TestLvkAnimation::testSwapAframes()
{
    LvkAnimation ani(1, "spin", 0);
    ani.addAframe(LvkAframe(10, 100, 200));
    ani.addAframe(LvkAframe(20, 101, 250));
    ani.addAframe(LvkAframe(30, 102, 300));
    ani.swapAframes(10, 30);
    // The CONTENT swaps; the ids stay position-stable. save() serializes
    // aframes sorted by id, so ids must always denote list positions or a
    // GUI reorder would be silently undone by the next save/load.
    QCOMPARE(ani._aframes.at(0).id, 10);
    QCOMPARE(ani._aframes.at(0).frameId, 102);
    QCOMPARE(ani._aframes.at(0).delay, 300);
    QCOMPARE(ani._aframes.at(1).id, 20);
    QCOMPARE(ani._aframes.at(1).frameId, 101);
    QCOMPARE(ani._aframes.at(2).id, 30);
    QCOMPARE(ani._aframes.at(2).frameId, 100);
    QCOMPARE(ani._aframes.at(2).delay, 200);
}

void TestLvkAnimation::testEqualityOperator()
{
    LvkAnimation a(1, "x", 0);
    LvkAnimation b(1, "x", 0);
    LvkAnimation c(1, "y", 0);
    QVERIFY(a == b);
    QVERIFY(!(a == c));
}

QTEST_GUILESS_MAIN(TestLvkAnimation)
#include "tst_lvkanimation.moc"
