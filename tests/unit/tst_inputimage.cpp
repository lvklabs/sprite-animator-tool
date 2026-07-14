// Unit tests for InputImage::toString / fromString.
//
// The legacy parser accepts 2- or 3-field comma-separated forms; both must
// continue to round-trip. We intentionally do not exercise pixmap loading
// (no GUI, no fixture files on disk for most cases) -- the format contract
// is what matters here.

#include <QtTest/QtTest>

#include "inputimage.h"

class TestInputImage : public QObject
{
    Q_OBJECT

private slots:
    void testToStringEmitsThreeFields();
    void testFromStringThreeFieldsRoundTrip();
    void testFromStringTwoFieldLegacyAcceptedScaleDefaultsToOne();
    void testFromStringRejectsOutOfRangeScale();
    void testFromStringEmptyFilename();
    void testFromStringSpecialCharsInFilename();
    void testFromStringRejectsTooFewFields();
    void testFromStringRejectsTooManyFields();
    void testEqualityOperator();
};

void TestInputImage::testToStringEmitsThreeFields()
{
    InputImage img(42, "ignored-path.png", 1.0);
    const QString s = img.toString();
    QCOMPARE(s.count(','), 2);
    QVERIFY(s.startsWith("42,"));
    QVERIFY(s.endsWith(",1"));
}

void TestInputImage::testFromStringThreeFieldsRoundTrip()
{
    // We can't compare pixmaps reliably without disk fixtures, so compare
    // the trio of fields that toString writes. Phase 4 InputImage rejects
    // absolute / UNC / NUL / ".." filenames, so we use a relative test
    // path (matches what the real on-disk .lvks fixtures look like).
    InputImage parsed;
    QVERIFY(parsed.fromString("7,relative/path/file.png,2"));
    QCOMPARE(parsed.id, 7);
    QCOMPARE(parsed.filename, QString("relative/path/file.png"));
    QCOMPARE(parsed.scale(), 2.0);

    // round-trip the serialized form back through fromString and re-check
    InputImage second;
    QVERIFY(second.fromString(parsed.toString()));
    QCOMPARE(second.id, parsed.id);
    QCOMPARE(second.filename, parsed.filename);
    QCOMPARE(second.scale(), parsed.scale());
}

void TestInputImage::testFromStringTwoFieldLegacyAcceptedScaleDefaultsToOne()
{
    // Phase 4: relative path (matches examples/*.lvks; absolute paths are
    // now rejected by the security validator).
    InputImage parsed;
    QVERIFY(parsed.fromString("3,some/file.png"));
    QCOMPARE(parsed.id, 3);
    QCOMPARE(parsed.filename, QString("some/file.png"));
    QCOMPARE(parsed.scale(), 1.0);
}

void TestInputImage::testFromStringRejectsOutOfRangeScale()
{
    // Round 7: the scale factor is bounded (kMinScale..kMaxScale). A
    // hand-edited/malicious value (huge, non-positive, NaN, non-numeric)
    // previously flowed unchecked into width()*scale -- a double->int
    // conversion that is UB when out of range, and an OOM-scale
    // allocation request for merely large values.
    InputImage parsed;
    QVERIFY(!parsed.fromString("0,seed.png,1e18"));
    QVERIFY(!parsed.fromString("0,seed.png,0"));
    QVERIFY(!parsed.fromString("0,seed.png,-3"));
    QVERIFY(!parsed.fromString("0,seed.png,nan"));
    QVERIFY(!parsed.fromString("0,seed.png,abc"));
    QVERIFY(!parsed.fromString(QStringLiteral("0,seed.png,%1")
                                   .arg(InputImage::kMaxScale * 2)));

    // Boundary values are accepted.
    QVERIFY(parsed.fromString(QStringLiteral("0,seed.png,%1").arg(InputImage::kMaxScale)));
    QCOMPARE(parsed.scale(), InputImage::kMaxScale);
    QVERIFY(parsed.fromString("0,seed.png,0.5"));
    QCOMPARE(parsed.scale(), 0.5);
}

void TestInputImage::testFromStringEmptyFilename()
{
    InputImage parsed;
    QVERIFY(parsed.fromString("12,,1.5"));
    QCOMPARE(parsed.id, 12);
    QVERIFY(parsed.filename.isEmpty());
    QCOMPARE(parsed.scale(), 1.5);
}

void TestInputImage::testFromStringSpecialCharsInFilename()
{
    // The .lvks contract forbids ',' in filenames but other special chars
    // (spaces, parentheses, percent signs) are fair game. Phase 4: paths
    // must be relative, so the test path no longer leads with '/'.
    const QString tricky = "path with spaces & (parens)_50%.png";
    InputImage parsed;
    QVERIFY(parsed.fromString(QString("99,%1,1").arg(tricky)));
    QCOMPARE(parsed.id, 99);
    QCOMPARE(parsed.filename, tricky);
    QCOMPARE(parsed.scale(), 1.0);
}

void TestInputImage::testFromStringRejectsTooFewFields()
{
    InputImage parsed;
    QVERIFY(!parsed.fromString("only-one-token"));
}

void TestInputImage::testFromStringRejectsTooManyFields()
{
    InputImage parsed;
    QVERIFY(!parsed.fromString("1,foo,2,extra"));
}

void TestInputImage::testEqualityOperator()
{
    InputImage a(5, "x.png", 1.0);
    InputImage b(5, "x.png", 1.0);
    InputImage c(5, "x.png", 2.0);
    QVERIFY(a == b);
    QVERIFY(!(a == c));
}

QTEST_GUILESS_MAIN(TestInputImage)
#include "tst_inputimage.moc"
