// tst_load_truncated.cpp (Round 7)
//
// SpriteState::load() must REJECT truncated and non-.lvks input instead
// of "successfully" loading partial data. Before the Round 7 fix, the
// parse loop broke on EOF regardless of parser state, so:
//   - a zero-byte file loaded fine (empty sprite),
//   - a file cut inside an unclosed images(/frames(/animations(/aframes(
//     block silently dropped everything after the cut,
//   - a completely foreign text file loaded as an empty sprite,
// and a headless `--export` of any of these exited 0 with empty/partial
// artifacts. The loader now fails with ErrInvalidFormat when EOF is
// reached before the version header or inside an unterminated block.

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>

#include "spritestate.h"

class TstLoadTruncated : public QObject
{
    Q_OBJECT

private slots:
    void emptyFileFails();
    void foreignTextFileFails();
    void cutBeforeVersionHeaderFails();
    void cutInsideImagesBlockFails();
    void cutInsideAframesBlockFails();
    void versionHeaderOnlyLoads();
    void wellFormedMinimalFileLoads();
    void truncatedRealExampleFails();

private:
    static QString writeFixture(const QTemporaryDir &dir, const QString &name,
                                const QByteArray &content);
    static void expectLoadFailure(const QString &path);
};

QString TstLoadTruncated::writeFixture(const QTemporaryDir &dir, const QString &name,
                                       const QByteArray &content)
{
    const QString path = dir.path() + QDir::separator() + name;
    QFile f(path);
    if (!f.open(QFile::WriteOnly)) {
        return QString();
    }
    f.write(content);
    f.close();
    return path;
}

void TstLoadTruncated::expectLoadFailure(const QString &path)
{
    QVERIFY(!path.isEmpty());
    SpriteState st;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(!st.load(path, &err),
             qPrintable(QStringLiteral("load() unexpectedly accepted %1").arg(path)));
    QCOMPARE(err, SpriteState::ErrInvalidFormat);
}

void TstLoadTruncated::emptyFileFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    expectLoadFailure(writeFixture(dir, QStringLiteral("empty.lvks"), QByteArray()));
}

void TstLoadTruncated::foreignTextFileFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    expectLoadFailure(writeFixture(dir, QStringLiteral("foreign.lvks"),
                                   QByteArrayLiteral("This is not a sprite file.\n"
                                                     "Just some text.\n")));
}

void TstLoadTruncated::cutBeforeVersionHeaderFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Only the comment banner made it to disk -- the version header and
    // everything after it are gone.
    expectLoadFailure(writeFixture(dir, QStringLiteral("banner_only.lvks"),
                                   QByteArrayLiteral("### LvkSprite ##\n")));
}

void TstLoadTruncated::cutInsideImagesBlockFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    expectLoadFailure(writeFixture(dir, QStringLiteral("cut_images.lvks"),
                                   QByteArrayLiteral("### LvkSprite ##\n"
                                                     "LvkSprite version 0.4\n\n"
                                                     "images(\n"
                                                     "\t0,solo.png,1\n")));
}

void TstLoadTruncated::cutInsideAframesBlockFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    expectLoadFailure(writeFixture(dir, QStringLiteral("cut_aframes.lvks"),
                                   QByteArrayLiteral("### LvkSprite ##\n"
                                                     "LvkSprite version 0.4\n\n"
                                                     "images(\n"
                                                     "\t0,solo.png,1\n"
                                                     ")\n\n"
                                                     "frames(\n"
                                                     "\t0,solo,0,0,0,16,16\n"
                                                     ")\n\n"
                                                     "animations(\n"
                                                     "\t0,walk,0\n"
                                                     "\taframes(\n"
                                                     "\t\t0,0,200,0,0,0\n")));
}

void TstLoadTruncated::versionHeaderOnlyLoads()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A header with no blocks is a legal (empty) sprite.
    const QString path = writeFixture(dir, QStringLiteral("header_only.lvks"),
                                      QByteArrayLiteral("### LvkSprite ##\n"
                                                        "LvkSprite version 0.4\n"));
    QVERIFY(!path.isEmpty());
    SpriteState st;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(path, &err), qPrintable(SpriteState::errorMessage(err)));
    QVERIFY(st.images().isEmpty());
    QVERIFY(st.animations().isEmpty());
}

void TstLoadTruncated::wellFormedMinimalFileLoads()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFixture(dir, QStringLiteral("minimal.lvks"),
                                      QByteArrayLiteral("### LvkSprite ##\n"
                                                        "LvkSprite version 0.4\n\n"
                                                        "images(\n"
                                                        "\t0,solo.png,1\n"
                                                        ")\n\n"
                                                        "frames(\n"
                                                        "\t0,solo,0,0,0,16,16\n"
                                                        ")\n\n"
                                                        "animations(\n"
                                                        "\t0,walk,0\n"
                                                        "\taframes(\n"
                                                        "\t\t0,0,200,0,0,0\n"
                                                        "\t)\n"
                                                        ")\n\n"
                                                        "### End LvkSprite ##\n"));
    QVERIFY(!path.isEmpty());
    SpriteState st;
    SpriteState::SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(st.load(path, &err), qPrintable(SpriteState::errorMessage(err)));
    QCOMPARE(st.images().size(), 1);
    QCOMPARE(st.frames().size(), 1);
    QCOMPARE(st.animations().size(), 1);
}

void TstLoadTruncated::truncatedRealExampleFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Reproduce the smoke-audit repro: the first 200 bytes of
    // examples/mario.lvks end inside the images( block.
    QFile mario(QStringLiteral(LVK_EXAMPLES_DIR) + QStringLiteral("/mario.lvks"));
    QVERIFY(mario.open(QFile::ReadOnly));
    const QByteArray head = mario.read(200);
    mario.close();
    QVERIFY(head.size() == 200);

    expectLoadFailure(writeFixture(dir, QStringLiteral("truncated_mario.lvks"), head));
}

// QTEST_MAIN like the other format tests: SpriteState carries QPixmap
// members, which need a QGuiApplication (offscreen QPA in CI).
QTEST_MAIN(TstLoadTruncated)
#include "tst_load_truncated.moc"
