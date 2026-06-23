// tests/format/tst_save_atomic.cpp
//
// Team D2 (D2.2): SpriteState::save() must NOT destroy the original file
// when the save itself fails.
//
// Pre-D2 behavior: save() called QFile::open(filename, WriteOnly | Text)
// which truncates the target file AT OPEN TIME. The save loop then
// checked sawInvalidRecord AFTER iterating; on failure (e.g. an image
// filename containing a comma) save() returned false but the original
// file was already at zero bytes. User-visible regression: open a
// working sprite, type a comma into a filename, hit save -> the .lvks
// becomes empty and the data is gone.
//
// D2.2 fix: save() writes to "<filename>.save-tmp" and atomic-renames on
// success. On failure the tmp file is removed and the original is
// untouched. This test asserts:
//   1. After pre-writing a known-good .lvks containing the marker line
//      "MARKER_BYTES" to disk,
//   2. Calling save() with a state that contains an image filename
//      with a comma (the failure path)
//   3. Returns false AND
//   4. The original file's bytes are intact (the marker is still there).

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "spritestate.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

class TstSaveAtomic : public QObject
{
    Q_OBJECT
private slots:
    void preservesOriginalWhenSaveFails();
    void successfulSaveProducesNoLeftoverTmpFile();
};

void TstSaveAtomic::preservesOriginalWhenSaveFails()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString sprPath =
        tmpDir.path() + QDir::separator() + QStringLiteral("precious.lvks");

    // Pre-write a "known-good" .lvks. We use a syntactically valid
    // header section plus a unique marker comment line that the test
    // can hunt for. A real loader would also accept this file.
    const QByteArray originalBytes =
        "### LvkSprite ##########################\n"
        "LvkSprite version 0.4\n\n"
        "# MARKER_BYTES_PRESERVED\n"
        "images(\n"
        ")\n\n"
        "frames(\n"
        ")\n\n"
        "animations(\n"
        ")\n\n"
        "custom_header(\n"
        ")\n\n"
        "### End LvkSprite ##########################\n";
    {
        QFile f(sprPath);
        QVERIFY(f.open(QFile::WriteOnly));
        f.write(originalBytes);
        f.close();
    }

    // Build an in-memory SpriteState that will FAIL to save: a single
    // image whose filename contains a comma. InputImage::toString
    // refuses to serialize the comma; save() trips sawInvalidRecord and
    // returns false. Pre-D2 this still truncated `sprPath`.
    SpriteState st;
    InputImage img;
    img.id = 0;
    img.filename = QStringLiteral("bad,name.png");
    st.addImage(img);

    SpriteStateError err = SpriteState::ErrNone;
    const bool ok = st.save(sprPath, &err);
    QVERIFY2(!ok, "save() must return false when an image filename has a comma");
    QCOMPARE(err, SpriteState::ErrInvalidFormat);

    // D2.2 invariant: the ORIGINAL bytes are still on disk. The marker
    // comment line must be present (proves the file wasn't truncated and
    // it's not a partial-write of the new content).
    QFile after(sprPath);
    QVERIFY(after.exists());
    QVERIFY(after.open(QFile::ReadOnly));
    const QByteArray afterBytes = after.readAll();
    after.close();

    QCOMPARE(afterBytes, originalBytes);
    QVERIFY2(afterBytes.contains("MARKER_BYTES_PRESERVED"),
             "Original file content was destroyed by failed save()");

    // Belt-and-braces: the .save-tmp file should not be left behind on
    // a failed save (it would clutter the user's workspace and confuse
    // file-pickers showing a "precious.lvks.save-tmp" sibling).
    const QString tmpPath = sprPath + QStringLiteral(".save-tmp");
    QVERIFY2(!QFile::exists(tmpPath),
             "save() left a .save-tmp file behind after failure");
}

void TstSaveAtomic::successfulSaveProducesNoLeftoverTmpFile()
{
    // Positive control: a clean save() must put the new contents at the
    // original path and not leave the .save-tmp sibling.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString sprPath =
        tmpDir.path() + QDir::separator() + QStringLiteral("ok.lvks");

    SpriteState st;
    InputImage img;
    img.id = 0;
    img.filename = QStringLiteral("clean.png");
    st.addImage(img);

    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(st.save(sprPath, &err));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(QFile::exists(sprPath));

    // No leftover.
    const QString tmpPath = sprPath + QStringLiteral(".save-tmp");
    QVERIFY2(!QFile::exists(tmpPath),
             "Successful save() left a .save-tmp file behind");

    // And the file actually contains save() output (look for the header).
    QFile out(sprPath);
    QVERIFY(out.open(QFile::ReadOnly));
    const QByteArray bytes = out.readAll();
    out.close();
    QVERIFY(bytes.contains("LvkSprite version"));
}

QTEST_MAIN(TstSaveAtomic)
#include "tst_save_atomic.moc"
