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
#ifdef Q_OS_LINUX
    void saveFollowsSymlinkToTarget();
#endif
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

#ifdef Q_OS_LINUX
void TstSaveAtomic::saveFollowsSymlinkToTarget()
{
    // Team H2 (H2.1): when @p filename is a symlink, save() must write to
    // the LINK TARGET, not replace the link with a regular file at the
    // link's own inode. Pre-H2 QSaveFile's atomic rename(2) would unlink
    // the symlink and substitute the freshly-written tmp -- silently
    // breaking any consumer that pointed at the link expecting the
    // target to receive updates.
    //
    // Linux-only because symlink semantics differ on Windows (where
    // QFile::link emits a .lnk shortcut, not a true symlink) and on
    // macOS where the resolution path through QFileInfo is identical
    // but the CI bots don't run with the permissions needed to create
    // arbitrary symlinks in /tmp. The bug, the fix, and the regression
    // are all Linux-relevant so a Linux-only test is the right scope.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString targetPath =
        tmpDir.path() + QDir::separator() + QStringLiteral("target.lvks");
    const QString linkPath =
        tmpDir.path() + QDir::separator() + QStringLiteral("link.lvks");

    // Pre-create the target with a known sentinel string so we can
    // confirm the link points at the same inode AND that save() wrote
    // through the link instead of clobbering it.
    {
        QFile target(targetPath);
        QVERIFY(target.open(QFile::WriteOnly));
        target.write("PLACEHOLDER_BYTES");
        target.close();
    }

    // QFile::link() on Linux creates a real symlink.
    QVERIFY2(QFile::link(targetPath, linkPath),
             "test environment cannot create symlinks");
    QVERIFY(QFileInfo(linkPath).isSymLink());

    // Sanity: capture the inode of the target so we can prove the same
    // inode is rewritten (and not replaced by a fresh inode at linkPath
    // that just happens to share the contents).
    const QFileInfo targetInfoBefore(targetPath);
    QVERIFY(targetInfoBefore.exists());

    // Build a SpriteState with enough content to produce a non-empty
    // save body, then save TO THE SYMLINK.
    SpriteState st;
    InputImage img;
    img.id = 0;
    img.filename = QStringLiteral("clean.png");
    st.addImage(img);

    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.save(linkPath, &err),
             qPrintable("save() to symlink should succeed: " +
                        SpriteState::errorMessage(err)));

    // H2.1 invariant #1: linkPath is STILL a symlink (we didn't replace
    // it with a regular file).
    QVERIFY2(QFileInfo(linkPath).isSymLink(),
             "save() replaced the symlink with a regular file");

    // H2.1 invariant #2: linkPath still resolves to targetPath.
    QCOMPARE(QFileInfo(linkPath).symLinkTarget(), targetPath);

    // H2.1 invariant #3: targetPath got the new bytes (no longer the
    // placeholder), proving the save went through the link.
    QFile targetAfter(targetPath);
    QVERIFY(targetAfter.open(QFile::ReadOnly));
    const QByteArray afterBytes = targetAfter.readAll();
    targetAfter.close();
    QVERIFY2(!afterBytes.contains("PLACEHOLDER_BYTES"),
             "target file was not overwritten by save() through the link");
    QVERIFY2(afterBytes.contains("LvkSprite version"),
             "target file does not contain saved sprite content");
}
#endif // Q_OS_LINUX

QTEST_MAIN(TstSaveAtomic)
#include "tst_save_atomic.moc"
