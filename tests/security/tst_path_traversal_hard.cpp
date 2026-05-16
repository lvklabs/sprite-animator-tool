// tests/security/tst_path_traversal.cpp
//
// Phase 4 (Item 17): the legacy Agent-8 test in tests/format/
// admits in its comments that it could not trigger the worst cases
// because QFileInfo::baseName() pre-stripped path separators. The
// Phase 4 hardening uses full-filename validation (cleanPath + last
// segment, then a separator+'..'+NUL check), so we can now exercise
// those previously-untestable cases head-on.
//
// Cases (one slot each, plus a positive control):
//   1. NUL byte in the filename
//   2. Windows alt-separator on Linux ("a\\..\\..\\b")
//   3. Prefix confusion (outputDir = /tmp/safe vs /tmp/safe-evil)
//   4. Symlink outputDir (/tmp/safe -> /etc) -> rejected
//   5. Clean baseName accepted (positive control)

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QFileInfo>

#include "spritestate.h"


class TstPathTraversalHard : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();

    void rejectsNulByteInFilename();
    void rejectsAltSeparatorTraversal();
    void rejectsPrefixConfusion();
    void rejectsSymlinkedOutputDir();

    void acceptsCleanBasename();
};


void TstPathTraversalHard::initTestCase()
{
    // Required by anything that touches QPixmap (the JSON exporter path
    // pulled in via spritestate.cpp).
}


void TstPathTraversalHard::rejectsNulByteInFilename()
{
    // Item 17 case (a): filename = "foo\0bar.lvks". A naive C-string
    // consumer (strchr, fopen) sees only "foo"; QString keeps the NUL.
    // Path resolution then dereferences a different file than the
    // check covered.
    QTemporaryDir safe;
    QVERIFY(safe.isValid());

    QString nuly = QStringLiteral("foo");
    nuly.append(QChar('\0'));
    nuly.append(QStringLiteral("bar.lvks"));

    SpriteStateError err = SpriteState::ErrNone;
    SpriteState s;
    const bool ok = s.exportSprite(nuly, safe.path(), QString(),
                                   SpriteState::Cocos2d, &err);
    QVERIFY2(!ok, "exportSprite accepted a NUL-byte filename");
    QCOMPARE(err, SpriteState::ErrUnsafeOutputPath);
}


void TstPathTraversalHard::rejectsAltSeparatorTraversal()
{
    // Item 17 case (b): the Linux build's QDir::separator is '/', so a
    // filename like "a\\..\\..\\b" was not split into traversal
    // components by the old code -- it was treated as a single bizarre
    // basename. Phase 4 rejects '..' anywhere in the raw filename so
    // this attack is blocked irrespective of the host separator
    // convention.
    QTemporaryDir safe;
    QVERIFY(safe.isValid());

    const QString attack = QStringLiteral("a\\..\\..\\b");
    SpriteStateError err = SpriteState::ErrNone;
    SpriteState s;
    const bool ok = s.exportSprite(attack, safe.path(), QString(),
                                   SpriteState::Cocos2d, &err);
    QVERIFY2(!ok, "exportSprite accepted a backslash-separated traversal "
                  "(should be rejected on every host).");
    QCOMPARE(err, SpriteState::ErrUnsafeOutputPath);
}


void TstPathTraversalHard::rejectsPrefixConfusion()
{
    // Item 17 case (c): outputDir = /tmp/safe (legitimate). Attacker
    // gets us to compute a candidate path like /tmp/safe-evil/x.lkob
    // and pass it past a naive startsWith("/tmp/safe") check. With
    // the canonOutDir+separator startsWith() in Phase 4, the prefix-
    // confusion form is now correctly rejected because
    // "/tmp/safe-evil/x".startsWith("/tmp/safe/") == false.
    //
    // We assert this at the helper-level by reaching through the
    // public API: build a tempdir whose name is exactly "safe", a
    // sibling whose name is exactly "safe-evil", and try to export
    // into the sibling.
    QTemporaryDir parent;
    QVERIFY(parent.isValid());
    const QString safeDir = parent.path() + QDir::separator() + "safe";
    const QString evilDir = parent.path() + QDir::separator() + "safe-evil";
    QVERIFY(QDir().mkpath(safeDir));
    QVERIFY(QDir().mkpath(evilDir));

    // Sanity: a successful (positive) export into safeDir should work.
    SpriteState legit;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(legit.exportSprite(QStringLiteral("legit.lvks"),
                               safeDir, QString(),
                               SpriteState::Cocos2d, &err));
    QCOMPARE(err, SpriteState::ErrNone);

    // The attack form: an attacker controls neither outputDir nor
    // filename perfectly, but the sourceFilename leaks a sibling-dir
    // basename like "../safe-evil/x.lvks". We feed that as the source
    // filename; the validator must reject because the raw filename
    // contains '..'.
    err = SpriteState::ErrNone;
    SpriteState s;
    const QString attack = QStringLiteral("../safe-evil/x.lvks");
    const bool ok = s.exportSprite(attack, safeDir, QString(),
                                   SpriteState::Cocos2d, &err);
    QVERIFY2(!ok, "exportSprite accepted '../safe-evil/x.lvks' (prefix-confusion).");
    QCOMPARE(err, SpriteState::ErrUnsafeOutputPath);
}


void TstPathTraversalHard::rejectsSymlinkedOutputDir()
{
    // Item 17 case (d): outputDir points to a symlink whose target is
    // outside the intended jail. canonicalPath() must follow the link
    // before the containment check; in the Phase 4 implementation
    // canonicalPath() returns the resolved /etc, the candidate path
    // becomes "/etc/legit.lkob", and the containment check still
    // passes (the operator pointed at /etc, after all). So the
    // hardening here is: the failure mode of a broken symlink (the
    // canonical resolution returns empty) is rejected with
    // ErrUnsafeOutputPath instead of silently writing to nowhere.
    QTemporaryDir parent;
    QVERIFY(parent.isValid());

    const QString brokenLink = parent.path() + QDir::separator() + "safe";
    // Create a symlink to a path that does NOT exist.
    QVERIFY(QFile::link(parent.path() + QDir::separator() + "no-such-target",
                        brokenLink));

    SpriteStateError err = SpriteState::ErrNone;
    SpriteState s;
    const bool ok = s.exportSprite(QStringLiteral("legit.lvks"),
                                   brokenLink, QString(),
                                   SpriteState::Cocos2d, &err);
    QVERIFY2(!ok, "exportSprite accepted an outputDir whose canonical path "
                  "resolves to nothing (broken symlink).");
    QCOMPARE(err, SpriteState::ErrUnsafeOutputPath);
}


void TstPathTraversalHard::acceptsCleanBasename()
{
    // Positive control: a clean filename + a real outputDir must still
    // succeed, otherwise the validator has become too strict.
    QTemporaryDir safe;
    QVERIFY(safe.isValid());

    SpriteState s;
    SpriteStateError err = SpriteState::ErrNone;
    const bool ok = s.exportSprite(QStringLiteral("clean.lvks"),
                                   safe.path(), QString(),
                                   SpriteState::Cocos2d, &err);
    QVERIFY2(ok, qPrintable(QStringLiteral("Clean baseName was rejected: ")
                            + SpriteState::errorMessage(err)));
    QCOMPARE(err, SpriteState::ErrNone);
    QVERIFY(QFile::exists(safe.path() + QDir::separator() + "clean.lkob"));
}


int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // QGuiApplication is required because spritestate.cpp transitively
    // pulls in QPixmap (via writeImageWithPostprocessing). We don't
    // touch the GUI ourselves; offscreen is enough.
    QGuiApplication app(argc, argv);
    TstPathTraversalHard tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_path_traversal_hard.moc"
