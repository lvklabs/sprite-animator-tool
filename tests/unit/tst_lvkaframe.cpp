// Unit tests for LvkAframe::toString / fromString.
//
// The serialized format is the most permissive of the data classes:
//
//   6 fields  (v0.4+):   id, frameId, delay, ox, oy, sticky
//   5 fields  (legacy):  id, frameId, delay, ox, oy            -> sticky=false
//   3 fields  (legacy):  id, frameId, delay                    -> ox=oy=0, sticky=false
//
// All three forms must parse. toString always writes the 6-field form.

#include <QtTest/QtTest>

#include "lvkaframe.h"

class TestLvkAframe : public QObject
{
    Q_OBJECT

private slots:
    void testToStringSixFields();
    void testFromStringSixFields();
    void testFromStringFiveFieldsDefaultsSticky();
    void testFromStringThreeFieldsDefaultsOffsetsAndSticky();
    void testStickyTrueRoundTrip();
    void testRoundTripFromToString();
    void testFromStringRejectsArityFour();
    void testFromStringRejectsArityTwo();
    void testEqualityOperator();
};

void TestLvkAframe::testToStringSixFields()
{
    LvkAframe a(1, 2, 200, 10, 20, true);
    const QString s = a.toString();
    QCOMPARE(s.count(','), 5);
    QCOMPARE(s, QString("1,2,200,10,20,1"));
}

void TestLvkAframe::testFromStringSixFields()
{
    LvkAframe a;
    QVERIFY(a.fromString("4,9,150,3,7,0"));
    QCOMPARE(a.id, 4);
    QCOMPARE(a.frameId, 9);
    QCOMPARE(a.delay, 150);
    QCOMPARE(a.ox, 3);
    QCOMPARE(a.oy, 7);
    QCOMPARE(a.sticky, false);
}

void TestLvkAframe::testFromStringFiveFieldsDefaultsSticky()
{
    LvkAframe a;
    QVERIFY(a.fromString("4,9,150,3,7"));
    QCOMPARE(a.id, 4);
    QCOMPARE(a.frameId, 9);
    QCOMPARE(a.delay, 150);
    QCOMPARE(a.ox, 3);
    QCOMPARE(a.oy, 7);
    QCOMPARE(a.sticky, false);
}

void TestLvkAframe::testFromStringThreeFieldsDefaultsOffsetsAndSticky()
{
    LvkAframe a;
    QVERIFY(a.fromString("0,5,100"));
    QCOMPARE(a.id, 0);
    QCOMPARE(a.frameId, 5);
    QCOMPARE(a.delay, 100);
    QCOMPARE(a.ox, 0);
    QCOMPARE(a.oy, 0);
    QCOMPARE(a.sticky, false);
}

void TestLvkAframe::testStickyTrueRoundTrip()
{
    LvkAframe original(11, 22, 33, 44, 55, true);
    LvkAframe parsed;
    QVERIFY(parsed.fromString(original.toString()));
    QCOMPARE(parsed.sticky, true);
    QVERIFY(parsed == original);
}

void TestLvkAframe::testRoundTripFromToString()
{
    LvkAframe original(7, 8, 250, -5, -10, false);
    LvkAframe parsed;
    QVERIFY(parsed.fromString(original.toString()));
    QVERIFY(parsed == original);
}

void TestLvkAframe::testFromStringRejectsArityFour()
{
    LvkAframe a;
    QVERIFY(!a.fromString("1,2,3,4"));
}

void TestLvkAframe::testFromStringRejectsArityTwo()
{
    LvkAframe a;
    QVERIFY(!a.fromString("1,2"));
}

void TestLvkAframe::testEqualityOperator()
{
    LvkAframe a(1, 2, 3, 4, 5, false);
    LvkAframe b(1, 2, 3, 4, 5, false);
    LvkAframe c(1, 2, 3, 4, 5, true);
    QVERIFY(a == b);
    QVERIFY(!(a == c));
}

QTEST_GUILESS_MAIN(TestLvkAframe)
#include "tst_lvkaframe.moc"
