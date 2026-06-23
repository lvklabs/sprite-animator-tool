// tst_unicode_names.cpp
//
// Phase 6b (Item 26): non-ASCII / comma-safe animation and frame names.
//
// Coverage:
//   * getMacroName produces a non-empty, ASCII-only, valid-C-identifier
//     output for a Unicode-only animation name (e.g. "飛び蹴り").
//   * Two animation names that sanitize to the same base macro get
//     disambiguating "_2" / "_3" suffixes in the generated header.
//   * LvkAnimation::toString / LvkFrame::toString / InputImage::toString
//     refuse to serialize a name containing a literal comma (the CSV
//     field separator); they return an empty QString and log a warning.
//
// We exercise the static `getMacroName` indirectly through the public
// exportSprite() path -- it's the only callable path that ends up writing
// macro names, and it's also the path that exercises the new collision
// disambiguation in the call site loop.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include "spritestate.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

class TstUnicodeNames : public QObject
{
    Q_OBJECT

private slots:
    void unicodeAnimationNameYieldsAsciiMacro();
    void collidingMacroNamesGetUniqueSuffixes();
    void toStringRejectsAnimationNameWithComma();
    void toStringRejectsFrameNameWithComma();
    void toStringRejectsImageFilenameWithComma();
    // Phase B1.2: save() must propagate the toString rejection as a
    // false return value rather than silently emitting a blank record
    // and pretending the save succeeded.
    void saveReturnsFalseOnAnimationNameWithComma();
    void saveReturnsFalseOnImageFilenameWithComma();

private:
    // Load the header file written by exportSprite and return its full text.
    static QString readHeader(const QString& dir, const QString& baseName);

    // Populate `st` with a minimal valid SpriteState: one image, one
    // frame, and a single animation with the supplied display name.
    // SpriteState inherits QObject, which disables copy/move, so we
    // populate-in-place rather than returning by value.
    static void fillMinimalStateWithAnimationName(SpriteState& st,
                                                  const QString& name);
};

QString TstUnicodeNames::readHeader(const QString& dir, const QString& baseName)
{
    // exportSprite writes the C header as AnimNameDef_<basename>.h
    // (see spritestate.cpp::exportSprite).  baseName here is the stem
    // without the .lvks suffix or any directory.
    const QString headerPath = dir + QDir::separator()
        + QStringLiteral("AnimNameDef_") + baseName + QStringLiteral(".h");
    QFile f(headerPath);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        return QString();
    }
    return QString::fromUtf8(f.readAll());
}

void TstUnicodeNames::fillMinimalStateWithAnimationName(SpriteState& st,
                                                        const QString& name)
{
    InputImage img;
    img.id = 0;
    img.filename = QStringLiteral("nonexistent.png");
    st.addImage(img);

    LvkFrame frame(0, /*imgId=*/0, /*ox=*/0, /*oy=*/0, /*w=*/16, /*h=*/16,
                   QStringLiteral("solo"));
    st.addFrame(frame);

    LvkAnimation ani(0, name, /*flags=*/0);
    LvkAframe af(0, /*frameId=*/0, /*delay=*/100, /*ox=*/0, /*oy=*/0,
                 /*sticky=*/false);
    ani.addAframe(af);
    st.addAnimation(ani);
}

void TstUnicodeNames::unicodeAnimationNameYieldsAsciiMacro()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Japanese name: "tobi-geri" ("flying kick").  Pre-Phase-6b this
    // round-tripped through toLatin1() and produced an empty macro,
    // yielding a "#define ANIM_" line with no symbol that the downstream
    // C compiler rejected.
    // 飛び蹴り - "tobi-geri" (flying kick) -- 4 Unicode codepoints, all
    // non-ASCII.  Each char's getMacroName output is '_', collapsed runs
    // of '_' reduce to a single '_', and the resulting bare '_' triggers
    // the "UNNAMED" fallback.  Two such animations would collide on
    // "UNNAMED" without the disambiguating-suffix logic.
    const QString kanji = QString::fromUtf8(
        "\xe9\xa3\x9b"   // 飛
        "\xe3\x81\xb3"   // び
        "\xe8\xb9\xb4"   // 蹴
        "\xe3\x82\x8a"); // り
    SpriteState st;
    fillMinimalStateWithAnimationName(st, kanji);

    const QString baseName = QStringLiteral("unicode_anim.lvks");
    QVERIFY(st.exportSprite(baseName, tmpDir.path(), QString(),
                            SpriteState::Cocos2d, nullptr));

    const QString header = readHeader(tmpDir.path(), QStringLiteral("unicode_anim"));
    QVERIFY2(!header.isEmpty(), "header file was not produced");

    // Look for the `#define ANIM_<symbol>` line.  The symbol must:
    //   1) be non-empty (no "#define ANIM_  \"...\""),
    //   2) contain only ASCII identifier characters [A-Za-z0-9_],
    //   3) appear on a "#define ANIM_<X>" line.
    QString animSymbol;
    const QStringList lines = header.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("#define ANIM_")) &&
            !line.contains(QStringLiteral("_FLAGS"))) {
            // "#define ANIM_<sym>\t..." -- pull out <sym>.
            QString rest = line.mid(QStringLiteral("#define ANIM_").size());
            int sep = -1;
            for (int i = 0; i < rest.size(); ++i) {
                const QChar c = rest.at(i);
                if (!(c.isLetterOrNumber() || c == QLatin1Char('_'))) {
                    sep = i;
                    break;
                }
            }
            animSymbol = (sep < 0) ? rest : rest.left(sep);
            break;
        }
    }

    QVERIFY2(!animSymbol.isEmpty(),
             "no `#define ANIM_<sym>` line found in generated header");

    // ASCII-only.
    for (QChar ch : animSymbol) {
        QVERIFY2(ch.unicode() < 128,
                 qPrintable(QString("non-ASCII char U+%1 in macro symbol '%2'")
                                .arg(ch.unicode(), 4, 16, QLatin1Char('0'))
                                .arg(animSymbol)));
    }

    // Valid C identifier: first char is a letter or '_', remainder is
    // letter/digit/'_'.  This is what the downstream game project's
    // C compiler will demand.
    const QChar first = animSymbol.at(0);
    QVERIFY2(first.isLetter() || first == QLatin1Char('_'),
             qPrintable(QString("macro symbol '%1' starts with non-identifier char")
                            .arg(animSymbol)));
    for (QChar ch : animSymbol) {
        QVERIFY(ch.isLetterOrNumber() || ch == QLatin1Char('_'));
    }
}

void TstUnicodeNames::collidingMacroNamesGetUniqueSuffixes()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Two animations whose names both collapse to the same base macro.
    // "fly" -> "FLY"; "FLY" -> "FLY"; without disambiguation we'd emit
    // two `#define ANIM_FLY` lines and the header would not compile.
    SpriteState st;

    InputImage img;
    img.id = 0;
    img.filename = QStringLiteral("nonexistent.png");
    st.addImage(img);

    LvkFrame frame(0, 0, 0, 0, 16, 16, QStringLiteral("solo"));
    st.addFrame(frame);

    LvkAnimation a1(0, QStringLiteral("fly"), 0);
    a1.addAframe(LvkAframe(0, 0, 100, 0, 0, false));
    st.addAnimation(a1);

    LvkAnimation a2(1, QStringLiteral("FLY"), 0);
    a2.addAframe(LvkAframe(0, 0, 100, 0, 0, false));
    st.addAnimation(a2);

    const QString baseName = QStringLiteral("collide.lvks");
    QVERIFY(st.exportSprite(baseName, tmpDir.path(), QString(),
                            SpriteState::Cocos2d, nullptr));

    const QString header = readHeader(tmpDir.path(), QStringLiteral("collide"));
    QVERIFY2(!header.isEmpty(), "header file was not produced");

    // Collect all `#define ANIM_<sym>` lines (excluding _FLAGS).
    QStringList symbols;
    const QStringList lines = header.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("#define ANIM_")) &&
            !line.contains(QStringLiteral("_FLAGS"))) {
            QString rest = line.mid(QStringLiteral("#define ANIM_").size());
            int sep = -1;
            for (int i = 0; i < rest.size(); ++i) {
                const QChar c = rest.at(i);
                if (!(c.isLetterOrNumber() || c == QLatin1Char('_'))) {
                    sep = i;
                    break;
                }
            }
            symbols.append((sep < 0) ? rest : rest.left(sep));
        }
    }

    QCOMPARE(symbols.size(), 2);
    QVERIFY2(symbols.at(0) != symbols.at(1),
             qPrintable(QString("collision: both animations produced '%1'")
                            .arg(symbols.at(0))));

    // The second one should carry the disambiguating suffix.
    QVERIFY2(symbols.at(1).endsWith(QStringLiteral("_2")),
             qPrintable(QString("second symbol '%1' missing _2 disambiguator")
                            .arg(symbols.at(1))));
}

void TstUnicodeNames::toStringRejectsAnimationNameWithComma()
{
    LvkAnimation ani(7, QStringLiteral("salto, mortal"), /*flags=*/0);
    const QString s = ani.toString();
    QVERIFY2(s.isEmpty(),
             qPrintable(QString("expected empty serialization, got '%1'").arg(s)));

    // A clean name still serializes.
    LvkAnimation clean(7, QStringLiteral("salto-mortal"), /*flags=*/0);
    QVERIFY(!clean.toString().isEmpty());
}

void TstUnicodeNames::toStringRejectsFrameNameWithComma()
{
    LvkFrame f(3, /*imgId=*/0, /*ox=*/0, /*oy=*/0, /*w=*/16, /*h=*/16,
               QStringLiteral("bad,name"));
    QVERIFY(f.toString().isEmpty());

    LvkFrame g(3, /*imgId=*/0, /*ox=*/0, /*oy=*/0, /*w=*/16, /*h=*/16,
               QStringLiteral("good_name"));
    QVERIFY(!g.toString().isEmpty());
}

void TstUnicodeNames::toStringRejectsImageFilenameWithComma()
{
    InputImage img(5, QStringLiteral("evil,name.png"), /*scale=*/1.0);
    QVERIFY(img.toString().isEmpty());

    InputImage ok(5, QStringLiteral("good_name.png"), /*scale=*/1.0);
    QVERIFY(!ok.toString().isEmpty());
}

void TstUnicodeNames::saveReturnsFalseOnAnimationNameWithComma()
{
    // B1.2: A SpriteState that contains one animation named "a,b"
    // (literal comma in the name) cannot be losslessly serialized in the
    // CSV-based on-disk format. The toString() rejection (returning an
    // empty QString) used to be silently consumed by save(), which would
    // then emit "\t\n" where the record should be -- a bare blank line.
    // Reload's fromString("") failed and the loader silently dropped the
    // record while save() still reported success. The fix is to detect
    // the empty record at save time and return false with an error code
    // the caller can surface.
    //
    // Team D3.2: in addition to the return-value check, prove the
    // failed save did NOT clobber a pre-existing good file at the
    // target path. The user invariant we want is: if save() returns
    // false, the previous on-disk content (if any) is untouched.
    // That's the property D2.2's atomic-save fix establishes
    // (write-to-tmp + rename-into-place; rename only happens if the
    // body completed cleanly). Without D2.2, save() opens the target
    // with truncate semantics and partial bytes leak through. We
    // assert the strong invariant: after the failed save, EITHER the
    // file does not exist OR its bytes match the pre-written content.
    // Asserting `size()>0` alone (the old form) is compatible with
    // arbitrary half-written garbage, which is the bug.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    SpriteState st;
    fillMinimalStateWithAnimationName(st, QStringLiteral("a,b"));

    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("bad_animation.lvks");

    // Pre-write a known-good .lvks at the target. We capture its bytes
    // verbatim so we can assert byte-identity after the failed save.
    // The content is intentionally a complete (parseable) v0.1 file so
    // a follow-up reload could check it survived as a real .lvks.
    const QByteArray preWritten =
        QByteArrayLiteral("### LvkSprite ##\n"
                          "LvkSprite version 0.1\n\n"
                          "images(\n\t0,clean.png\n)\n\n"
                          "frames(\n\t0,solo,0,0,0,16,16\n)\n\n"
                          "animations(\n\t0,clean\n"
                          "\taframes(\n\t\t0,0,200\n\t)\n"
                          ")\n\n"
                          "### End LvkSprite ##\n");
    {
        QFile pre(out);
        QVERIFY(pre.open(QFile::WriteOnly | QFile::Truncate));
        QCOMPARE(pre.write(preWritten), static_cast<qint64>(preWritten.size()));
        pre.close();
    }
    QVERIFY(QFile::exists(out));

    SpriteStateError err = SpriteState::ErrNone;
    const bool ok = st.save(out, &err);
    QVERIFY2(!ok, "save() must return false when an animation name contains a comma");
    QCOMPARE(err, SpriteState::ErrInvalidFormat);

    // Atomic-save invariant: either the file is gone (target removed
    // because the rename never happened) OR its bytes still equal the
    // pre-written content. Anything else means save() half-wrote bytes
    // through the target and clobbered the user's previous file --
    // exactly the silent-data-loss regression we are guarding against.
    //
    // NOTE: this assertion is the D3.2 strengthening that depends on
    // D2.2's atomic-save fix (QSaveFile-style write-to-tmp + rename).
    // In worktrees where D2.2 is not yet in place, save() opens the
    // target with truncate semantics and partial bytes leak through;
    // the QEXPECT_FAIL below documents that dependency without
    // turning the suite red. Once D2.2 lands, the QEXPECT_FAIL can be
    // dropped and this assertion will hold unconditionally.
    QFileInfo postInfo(out);
    if (postInfo.exists()) {
        QFile post(out);
        QVERIFY(post.open(QFile::ReadOnly));
        const QByteArray postBytes = post.readAll();
        QEXPECT_FAIL("", "depends on D2.2 atomic save (QSaveFile/rename)", Continue);
        QVERIFY2(postBytes == preWritten,
                 "failed save() must not clobber pre-existing good content");

        // Even without D2.2, we can assert the WEAKER but still
        // meaningful invariant: a reload of whatever is on disk must
        // not silently re-introduce the bad "a,b" animation as a
        // valid record.
        SpriteState reloaded;
        SpriteStateError loadErr = SpriteState::ErrNone;
        const bool loadedOk = reloaded.load(out, &loadErr);
        if (loadedOk) {
            // The bad animation MUST NOT have round-tripped: either
            // because save() never wrote it OR because the post-save
            // file is the pre-written good file with a different
            // animation. The exact identity depends on D2.2; what we
            // forbid here is the original silent-data-loss bug where
            // an animation named "a,b" would survive intact.
            for (auto it = reloaded.animations().constBegin();
                 it != reloaded.animations().constEnd(); ++it) {
                QVERIFY2(it.value().name != QStringLiteral("a,b"),
                         "the comma-bearing animation must not survive save+reload");
            }
        }
        // (else: load failed, which is also acceptable.)
    }
}

void TstUnicodeNames::saveReturnsFalseOnImageFilenameWithComma()
{
    // Same invariant as saveReturnsFalseOnAnimationNameWithComma but for
    // the images section: an image filename with a comma cannot be
    // serialized; save() must signal failure rather than silently
    // emitting a blank image record.
    //
    // Team D3.2: also assert the atomic-save invariant on a pre-
    // written good file at the target path.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    SpriteState st;
    InputImage img;
    img.id = 0;
    img.filename = QStringLiteral("evil,name.png");
    st.addImage(img);

    LvkFrame frame(0, 0, 0, 0, 16, 16, QStringLiteral("solo"));
    st.addFrame(frame);

    LvkAnimation ani(0, QStringLiteral("clean"), 0);
    ani.addAframe(LvkAframe(0, 0, 100, 0, 0, false));
    st.addAnimation(ani);

    const QString out = tmpDir.path() + QDir::separator()
        + QStringLiteral("bad_image.lvks");

    const QByteArray preWritten =
        QByteArrayLiteral("### LvkSprite ##\n"
                          "LvkSprite version 0.1\n\n"
                          "images(\n\t0,clean.png\n)\n\n"
                          "frames(\n\t0,solo,0,0,0,16,16\n)\n\n"
                          "animations(\n\t0,clean\n"
                          "\taframes(\n\t\t0,0,200\n\t)\n"
                          ")\n\n"
                          "### End LvkSprite ##\n");
    {
        QFile pre(out);
        QVERIFY(pre.open(QFile::WriteOnly | QFile::Truncate));
        QCOMPARE(pre.write(preWritten), static_cast<qint64>(preWritten.size()));
        pre.close();
    }
    QVERIFY(QFile::exists(out));

    SpriteStateError err = SpriteState::ErrNone;
    const bool ok = st.save(out, &err);
    QVERIFY2(!ok, "save() must return false when an image filename contains a comma");
    QCOMPARE(err, SpriteState::ErrInvalidFormat);

    // Same atomic-save invariant as saveReturnsFalseOnAnimationNameWithComma.
    // QEXPECT_FAIL until D2.2 lands; see the long comment in that test.
    if (QFile::exists(out)) {
        QFile post(out);
        QVERIFY(post.open(QFile::ReadOnly));
        const QByteArray postBytes = post.readAll();
        QEXPECT_FAIL("", "depends on D2.2 atomic save (QSaveFile/rename)", Continue);
        QVERIFY2(postBytes == preWritten,
                 "failed save() must not clobber pre-existing good content");
    }
}

QTEST_MAIN(TstUnicodeNames)
#include "tst_unicode_names.moc"
