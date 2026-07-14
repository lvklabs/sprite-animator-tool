// tst_image_content_sniff.cpp (Round 7)
//
// lvk::validateImageFile()'s CONTENT-sniffing branches. The coverage
// audit found the existing whitelist tests (tst_lvks_image_load_whitelist,
// tst_image_path_injection) all stop at the earlier extension gate: the
// QImageReader sniff for a PRESENT file -- the defense against a
// malicious payload renamed to a whitelisted extension -- never executed
// in any test, and neither did ExistenceCheck::Required's missing-file
// rejection.

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QString>
#include <QTemporaryDir>

#include "image_validation.h"

class TstImageContentSniff : public QObject
{
    Q_OBJECT

private slots:
    void garbageBytesNamedPngAreRejected();
    void truncatedPngIsRejected();
    void mismatchedButWhitelistedContentIsAccepted();
    void genuinePngIsAccepted();
    void requiredModeRejectsMissingFile();
    void optionalModeAdmitsMissingFileWithGoodExtension();
    void optionalModeStillSniffsPresentFile();
};

void TstImageContentSniff::garbageBytesNamedPngAreRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/fake.png");
    {
        QFile f(path);
        QVERIFY(f.open(QFile::WriteOnly));
        f.write(QByteArrayLiteral("this is definitely not a PNG payload"));
    }

    QString errMsg;
    QVERIFY2(!lvk::validateImageFile(path, &errMsg, lvk::ExistenceCheck::Required),
             "garbage bytes with a .png name passed the content sniff");
    QVERIFY(!errMsg.isEmpty());
}

void TstImageContentSniff::truncatedPngIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // A real PNG cut after the first 12 bytes: the signature survives but
    // the stream is undecodable. QImageReader must not classify it as a
    // loadable image.
    QByteArray realPng;
    {
        QImage pixels(8, 8, QImage::Format_ARGB32);
        pixels.fill(Qt::green);
        const QString whole = dir.path() + QStringLiteral("/whole.png");
        QVERIFY(pixels.save(whole));
        QFile f(whole);
        QVERIFY(f.open(QFile::ReadOnly));
        realPng = f.readAll();
    }
    QVERIFY(realPng.size() > 12);

    const QString cutPath = dir.path() + QStringLiteral("/cut.png");
    {
        QFile f(cutPath);
        QVERIFY(f.open(QFile::WriteOnly));
        f.write(realPng.left(12));
    }

    QString errMsg;
    const bool ok = lvk::validateImageFile(cutPath, &errMsg, lvk::ExistenceCheck::Required);
    QVERIFY2(!ok, "a truncated PNG passed the content sniff");
}

void TstImageContentSniff::mismatchedButWhitelistedContentIsAccepted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // A genuine BMP named .png: both formats are whitelisted, so this is
    // a tolerated mislabel (the sniff exists to keep NON-whitelisted
    // payloads out, not to police extension spelling).
    const QString path = dir.path() + QStringLiteral("/mislabeled.png");
    {
        QImage pixels(8, 8, QImage::Format_RGB32);
        pixels.fill(Qt::yellow);
        QVERIFY(pixels.save(path, "BMP"));
    }

    QString errMsg;
    QVERIFY2(lvk::validateImageFile(path, &errMsg, lvk::ExistenceCheck::Required),
             qPrintable(QStringLiteral("BMP-named-.png rejected: ") + errMsg));
}

void TstImageContentSniff::genuinePngIsAccepted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/real.png");
    {
        QImage pixels(8, 8, QImage::Format_ARGB32);
        pixels.fill(Qt::red);
        QVERIFY(pixels.save(path));
    }

    QString errMsg;
    QVERIFY2(lvk::validateImageFile(path, &errMsg, lvk::ExistenceCheck::Required),
             qPrintable(QStringLiteral("genuine PNG rejected: ") + errMsg));
}

void TstImageContentSniff::requiredModeRejectsMissingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString errMsg;
    QVERIFY2(!lvk::validateImageFile(dir.path() + QStringLiteral("/absent.png"), &errMsg,
                                     lvk::ExistenceCheck::Required),
             "Required mode admitted a missing file");
    QVERIFY(!errMsg.isEmpty());
}

void TstImageContentSniff::optionalModeAdmitsMissingFileWithGoodExtension()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // The legitimate broken-asset-link case: record admitted, pixmap
    // will be null.
    QVERIFY(lvk::validateImageFile(dir.path() + QStringLiteral("/absent.png"), nullptr,
                                   lvk::ExistenceCheck::Optional));
    // ...but a smuggled non-whitelisted extension is still rejected even
    // when absent (the F1.2 TOCTOU fix).
    QVERIFY(!lvk::validateImageFile(dir.path() + QStringLiteral("/absent.eps"), nullptr,
                                    lvk::ExistenceCheck::Optional));
}

void TstImageContentSniff::optionalModeStillSniffsPresentFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/fake.png");
    {
        QFile f(path);
        QVERIFY(f.open(QFile::WriteOnly));
        f.write(QByteArrayLiteral("garbage payload pretending to be a png"));
    }

    // A PRESENT file gets the full content sniff even in Optional mode.
    QString errMsg;
    QVERIFY2(!lvk::validateImageFile(path, &errMsg, lvk::ExistenceCheck::Optional),
             "Optional mode skipped the content sniff for a present file");
}

QTEST_MAIN(TstImageContentSniff)
#include "tst_image_content_sniff.moc"
