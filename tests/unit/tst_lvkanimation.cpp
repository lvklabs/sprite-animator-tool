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

void TestLvkAnimation::testSwapAframes()
{
    LvkAnimation ani(1, "spin", 0);
    ani.addAframe(LvkAframe(10, 100, 200));
    ani.addAframe(LvkAframe(20, 101, 250));
    ani.addAframe(LvkAframe(30, 102, 300));
    ani.swapAframes(10, 30);
    QCOMPARE(ani._aframes.at(0).id, 30);
    QCOMPARE(ani._aframes.at(1).id, 20);
    QCOMPARE(ani._aframes.at(2).id, 10);
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
