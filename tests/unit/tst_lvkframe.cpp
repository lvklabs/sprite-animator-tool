// Unit tests for LvkFrame::toString / fromString.
//
// LvkFrame's serialized form is always seven comma-separated fields:
//   id,name,imgId,ox,oy,w,h
// Unlike the other data classes there is no legacy field-count fallback --
// any other arity is an error. We pin both the success and the rejection
// paths.

#include <QtTest/QtTest>

#include "lvkframe.h"

class TestLvkFrame : public QObject
{
    Q_OBJECT

private slots:
    void testToStringSevenFields();
    void testFromStringRoundTrip();
    void testRoundTripFromToString();
    void testRectAccessor();
    void testSetRect();
    void testFromStringRejectsArityFive();
    void testFromStringRejectsArityEight();
    void testEqualityOperator();
};

void TestLvkFrame::testToStringSevenFields()
{
    LvkFrame f(1, 2, 10, 20, 30, 40, "punch");
    const QString s = f.toString();
    QCOMPARE(s.count(','), 6);
    QCOMPARE(s, QString("1,punch,2,10,20,30,40"));
}

void TestLvkFrame::testFromStringRoundTrip()
{
    LvkFrame parsed;
    QVERIFY(parsed.fromString("5,kick,9,1,2,64,48"));
    QCOMPARE(parsed.id, 5);
    QCOMPARE(parsed.name, QString("kick"));
    QCOMPARE(parsed.imgId, 9);
    QCOMPARE(parsed.ox, 1);
    QCOMPARE(parsed.oy, 2);
    QCOMPARE(parsed.w, 64);
    QCOMPARE(parsed.h, 48);
}

void TestLvkFrame::testRoundTripFromToString()
{
    LvkFrame original(8, 3, -5, -7, 100, 200, "jump-up");
    LvkFrame parsed;
    QVERIFY(parsed.fromString(original.toString()));
    QVERIFY(parsed == original);
}

void TestLvkFrame::testRectAccessor()
{
    LvkFrame f(0, 0, 5, 6, 7, 8);
    QCOMPARE(f.rect(), QRect(5, 6, 7, 8));
}

void TestLvkFrame::testSetRect()
{
    LvkFrame f;
    f.setRect(QRect(11, 22, 33, 44));
    QCOMPARE(f.ox, 11);
    QCOMPARE(f.oy, 22);
    QCOMPARE(f.w, 33);
    QCOMPARE(f.h, 44);
}

void TestLvkFrame::testFromStringRejectsArityFive()
{
    LvkFrame f;
    QVERIFY(!f.fromString("1,foo,2,3,4"));
}

void TestLvkFrame::testFromStringRejectsArityEight()
{
    LvkFrame f;
    QVERIFY(!f.fromString("1,foo,2,3,4,5,6,7"));
}

void TestLvkFrame::testEqualityOperator()
{
    LvkFrame a(1, 2, 3, 4, 5, 6, "n");
    LvkFrame b(1, 2, 3, 4, 5, 6, "n");
    LvkFrame c(1, 2, 3, 4, 5, 6, "different");
    QVERIFY(a == b);
    QVERIFY(!(a == c));
}

QTEST_GUILESS_MAIN(TestLvkFrame)
#include "tst_lvkframe.moc"
