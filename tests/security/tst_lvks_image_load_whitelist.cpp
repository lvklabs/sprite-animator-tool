// tests/security/tst_lvks_image_load_whitelist.cpp
//
// Team D2 (D2.1): SpriteState::load() must enforce the SAME image-format
// whitelist that the GUI dialog enforces. Without this, a malicious .lvks
// file referencing an unsupported format (e.g. "evil.eps") survives
// InputImage::fromString's isSafeImagePath check (it's a clean relative
// path with no NUL / no traversal), gets inserted into state by
// SpriteState::load, and is rendered unchecked by the controller's
// refreshTable -- the "advisory dialog" hole B3 closed in the dialog
// path remained open through the load path.
//
// This test writes a real on-disk fixture: a minimal v0.4 .lvks file that
// references a fake "evil.eps" (a 1-byte file -- format-sniffing reports
// "eps" or empty, which is not on the whitelist). It then loads the file
// and asserts:
//   1. SpriteState::load() returns true (we do NOT abort the whole load
//      on a rejected record; the user gets a partial sprite).
//   2. The rejectedCount outparam is bumped to 1.
//   3. SpriteState ends up with ZERO images -- the malicious record was
//      dropped.

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

#include "spritestate.h"
#include "types.h"

class TstLvksImageLoadWhitelist : public QObject
{
    Q_OBJECT
private slots:
    void rejectsEpsRecordViaLoadPath();
    void acceptsValidPngRecordViaLoadPath();
    // Team F1 (F1.2): TOCTOU bypass. Pre-F1, an attacker .lvks
    // referencing "evil.eps" with the file ABSENT at load time was
    // silently admitted (the validator was skipped on missing files).
    // The new ExistenceCheck::Optional mode still gates on the
    // extension, so the .eps reference is rejected even when the file
    // hasn't been dropped yet.
    void rejectsMissingFileWithBadExtension();
    // Team F1 (F1.2): a .lvks referencing a missing file with a
    // GOOD extension (e.g. "moved.png" -- legitimate broken-asset
    // link) must still be admitted. The validator's Optional mode
    // tolerates absent files iff their extension is on the whitelist.
    void acceptsMissingFileWithGoodExtension();

private:
    // Build a v0.4 .lvks fixture referencing a single image filename, and
    // return the .lvks path. The image file itself is NOT written here --
    // the caller is responsible for creating it (or not) before load.
    static QString writeFixture(const QDir &dir,
                                const QString &imageFilename);
};

QString TstLvksImageLoadWhitelist::writeFixture(const QDir &dir,
                                                const QString &imageFilename)
{
    const QString path = dir.absoluteFilePath(QStringLiteral("attack.lvks"));
    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text)) {
        return QString();
    }
    QTextStream ts(&f);
    ts << "### LvkSprite ##########################\n";
    ts << "LvkSprite version 0.4\n\n";
    ts << "images(\n";
    ts << "\t0," << imageFilename << ",1\n";
    ts << ")\n\n";
    ts << "frames(\n";
    ts << ")\n\n";
    ts << "animations(\n";
    ts << ")\n\n";
    ts << "custom_header(\n";
    ts << ")\n\n";
    ts << "### End LvkSprite ##########################\n";
    f.close();
    return path;
}

void TstLvksImageLoadWhitelist::rejectsEpsRecordViaLoadPath()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir::setCurrent(tmp.path());

    // Create a fake "evil.eps": 1 byte of arbitrary content. QImageReader
    // will sniff this as either an empty / unknown format -- either way
    // it falls OUTSIDE the whitelist (png/jpg/jpeg/bmp/gif/webp/svg/xpm/
    // xbm/tif/tiff). The crucial property: the file EXISTS on disk, so
    // the loader's "skip whitelist when file is absent" tolerance for
    // broken-asset .lvks files does not apply.
    {
        QFile evil(QStringLiteral("evil.eps"));
        QVERIFY(evil.open(QFile::WriteOnly));
        evil.write("%"); // a single byte of PostScript-flavored garbage
        evil.close();
    }
    QVERIFY(QFileInfo(QStringLiteral("evil.eps")).exists());

    const QString lvksPath =
        writeFixture(QDir(tmp.path()), QStringLiteral("evil.eps"));
    QVERIFY(!lvksPath.isEmpty());

    SpriteState st;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    int rejectedCount = 0;
    const bool ok = st.load(lvksPath, &err, &rejectedCount);

    // D2.1: load returns true (partial success); the malicious record
    // was SKIPPED, not promoted to a hard parser error. The user gets a
    // sprite missing the bad image rather than a load failure.
    QVERIFY2(ok, "load() should return true even when image records are "
                 "rejected -- partial success is better than aborting");
    QCOMPARE(err, SpriteState::ErrNone);

    // The whole point: ZERO images in the loaded state. The evil.eps
    // record was caught by the format whitelist and dropped.
    QCOMPARE(st.images().size(), 0);

    // D2.3 contract: rejectedCount is the number of records skipped.
    QCOMPARE(rejectedCount, 1);
}

void TstLvksImageLoadWhitelist::acceptsValidPngRecordViaLoadPath()
{
    // Positive control: a clean PNG record on the load path is admitted
    // (the validator is not a blanket false-positive). This proves the
    // whitelist accepts the formats it's supposed to.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir::setCurrent(tmp.path());

    // Write a real PNG. QImage().save() produces a file QImageReader
    // sniffs as "png" -- on the whitelist.
    {
        QImage img(4, 4, QImage::Format_ARGB32);
        img.fill(qRgba(0, 255, 0, 255));
        QVERIFY(img.save(QStringLiteral("good.png"), "PNG"));
    }

    const QString lvksPath =
        writeFixture(QDir(tmp.path()), QStringLiteral("good.png"));
    QVERIFY(!lvksPath.isEmpty());

    SpriteState st;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    int rejectedCount = 0;
    const bool ok = st.load(lvksPath, &err, &rejectedCount);
    QVERIFY(ok);
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(st.images().size(), 1);
    QCOMPARE(rejectedCount, 0);
}

void TstLvksImageLoadWhitelist::rejectsMissingFileWithBadExtension()
{
    // Team F1 (F1.2): TOCTOU bypass. Build a .lvks that references
    // "evil.eps" -- a file that does NOT exist on disk. Pre-F1 this
    // record was admitted into state (the loader skipped the
    // whitelist entirely for absent files). An attacker could ship
    // the .lvks, then later drop evil.eps next to it; on the next
    // refresh the engine would decode it through Qt's image plugins.
    //
    // F1.2 fix: validate the EXTENSION even when the file is missing.
    // ".eps" is not on the whitelist, so the record must be rejected
    // before it reaches state, regardless of whether evil.eps shows
    // up later. The rejectedCount outparam is bumped.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir::setCurrent(tmp.path());

    // Intentionally do NOT create evil.eps. The whole point is to
    // exercise the file-absent code path.
    QVERIFY(!QFileInfo(QStringLiteral("evil.eps")).exists());

    const QString lvksPath =
        writeFixture(QDir(tmp.path()), QStringLiteral("evil.eps"));
    QVERIFY(!lvksPath.isEmpty());

    SpriteState st;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    int rejectedCount = 0;
    const bool ok = st.load(lvksPath, &err, &rejectedCount);

    // Partial-success contract: load() returns true (the rest of the
    // file parses fine), but the malicious record was caught.
    QVERIFY2(ok, "load() should return true even when image records are "
                 "rejected via the missing-file-bad-extension path");
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(st.images().size(), 0);
    QCOMPARE(rejectedCount, 1);
}

void TstLvksImageLoadWhitelist::acceptsMissingFileWithGoodExtension()
{
    // Team F1 (F1.2): tolerance for the legitimate broken-asset-link
    // case. Loading a .lvks whose image was moved/deleted but whose
    // extension is whitelisted (".png" / ".jpg" / etc.) should admit
    // the record, so the editor can still open the sprite (the user
    // can re-point the image filename through the GUI). Only the
    // pixmap will be null.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir::setCurrent(tmp.path());

    QVERIFY(!QFileInfo(QStringLiteral("moved.png")).exists());

    const QString lvksPath =
        writeFixture(QDir(tmp.path()), QStringLiteral("moved.png"));
    QVERIFY(!lvksPath.isEmpty());

    SpriteState st;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    int rejectedCount = 0;
    const bool ok = st.load(lvksPath, &err, &rejectedCount);
    QVERIFY(ok);
    QCOMPARE(err, SpriteState::ErrNone);
    QCOMPARE(st.images().size(), 1);
    QCOMPARE(rejectedCount, 0);
}

int main(int argc, char *argv[])
{
    // QPixmap / QImage need a QGuiApplication + a platform plugin.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TstLvksImageLoadWhitelist tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_lvks_image_load_whitelist.moc"
