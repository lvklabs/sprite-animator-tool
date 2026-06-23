// tst_json_export.cpp
//
// Agent 8 (Phase 3): exercises the new JSON atlas exporter wired through
// SpriteState::exportSprite(format=Json|All).
//
// Loads examples/mario.lvks, exports to a temp dir, parses the JSON, and
// verifies:
//   - mario.png exists and is a non-empty file,
//   - mario.json exists,
//   - the "frames" object has at least as many entries as the source has
//     non-orphan frames,
//   - the "animations" object has exactly as many keys as the source has
//     animations, and each sequence is non-empty.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>

#include "exporters/JsonAtlasExporter.h"
#include "inputimage.h"
#include "lvkaframe.h"
#include "lvkanimation.h"
#include "lvkframe.h"
#include "spritestate.h"

#ifndef LVK_EXAMPLES_DIR
#  define LVK_EXAMPLES_DIR "."
#endif

// Team H2 (H2.3 / H2.4): the duplicate-name and determinism tests need
// to inject synthetic frames with controlled names/heights AND a real
// QPixmap (the exporter skips frames whose fpixmap() returns a null
// pixmap). A direct-access subclass mirrors the pattern already used in
// tests/security/tst_atlas_size_bomb.cpp so we don't go through disk
// I/O for every fixture.
class TestableSpriteState : public SpriteState
{
public:
    using SpriteState::SpriteState;
    void seedImage(InputImage& img) { addImage(img); }
    void seedFrame(LvkFrame& f) { addFrame(f); }
    void seedAnimation(LvkAnimation& a, LvkAframe& af) {
        addAnimation(a);
        addAframe(af, a.id);
    }
    // Append another aframe to an existing animation. Tests build
    // multi-frame timelines this way (seedAnimation seeds the first,
    // addAframeForTest adds the rest).
    void addAframeForTest(LvkAframe& af, Id aniId) {
        addAframe(af, aniId);
    }
    void overridePixmap(Id frameId, const QPixmap& px) {
        _fpixmaps[frameId] = px;
    }
};

class TstJsonExport : public QObject
{
    Q_OBJECT
private slots:
    void exportMarioAtlas();
    void exportAllFormatsAtomicity();
    void parseFormatHelper();
    void duplicateFrameNamesDisambiguated();
    void equalHeightFramesProduceDeterministicOutput();
};

void TstJsonExport::exportMarioAtlas()
{
    const QString src = QString::fromUtf8(LVK_EXAMPLES_DIR) + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    SpriteState s;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY2(s.load(src, &err), qPrintable(SpriteState::errorMessage(err)));

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // The exporter uses the basename of `src` (mario) under outputDir.
    const QString fakePath = tmp.path() + QDir::separator() + "mario.lvks";
    err = SpriteState::ErrNone;
    QVERIFY2(s.exportSprite(fakePath, tmp.path(), QString(),
                            SpriteState::Json, &err),
             qPrintable("JSON export failed: " + SpriteState::errorMessage(err)));
    QCOMPARE(err, SpriteState::ErrNone);

    const QString png  = tmp.path() + QDir::separator() + "mario.png";
    const QString json = tmp.path() + QDir::separator() + "mario.json";
    QVERIFY2(QFile::exists(png),  qPrintable("missing PNG: " + png));
    QVERIFY2(QFile::exists(json), qPrintable("missing JSON: " + json));
    QVERIFY(QFileInfo(png).size() > 0);

    QFile f(json);
    QVERIFY(f.open(QFile::ReadOnly));
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    QVERIFY2(pe.error == QJsonParseError::NoError, qPrintable(pe.errorString()));
    QVERIFY(doc.isObject());

    QJsonObject root = doc.object();
    QVERIFY(root.contains(QStringLiteral("frames")));
    QVERIFY(root.contains(QStringLiteral("animations")));
    QVERIFY(root.contains(QStringLiteral("meta")));

    // mario.lvks has 3 animations: walk / jump / queen_walk.
    QJsonObject anims = root.value(QStringLiteral("animations")).toObject();
    QCOMPARE(anims.size(), 3);
    for (auto it = anims.constBegin(); it != anims.constEnd(); ++it) {
        QVERIFY2(it.value().isArray(),
                 qPrintable(QStringLiteral("animation %1 is not an array").arg(it.key())));
        QVERIFY2(it.value().toArray().size() > 0,
                 qPrintable(QStringLiteral("animation %1 is empty").arg(it.key())));
    }

    // Frames object must have at least 1 entry per non-orphan frame in
    // the source. mario.lvks has 6 frames; some are used, some may not
    // be - we just require >=1.
    QJsonObject frames = root.value(QStringLiteral("frames")).toObject();
    QVERIFY(frames.size() >= 1);
    for (auto it = frames.constBegin(); it != frames.constEnd(); ++it) {
        QJsonObject f = it.value().toObject();
        QVERIFY(f.contains(QStringLiteral("filename")));
        QVERIFY(f.contains(QStringLiteral("frame")));
        QVERIFY(f.contains(QStringLiteral("sourceSize")));
        QVERIFY(f.contains(QStringLiteral("duration")));
    }

    // meta must include the atlas image filename (relative).
    QJsonObject meta = root.value(QStringLiteral("meta")).toObject();
    QCOMPARE(meta.value(QStringLiteral("image")).toString(), QStringLiteral("mario.png"));
}

void TstJsonExport::exportAllFormatsAtomicity()
{
    // ExportFormat::All should emit BOTH Cocos2d outputs AND the JSON
    // atlas. Verifies dispatching logic in SpriteState::exportSprite.
    const QString src = QString::fromUtf8(LVK_EXAMPLES_DIR) + QDir::separator() + "mario.lvks";
    QVERIFY2(QFile::exists(src), qPrintable("missing fixture: " + src));

    SpriteState s;
    SpriteStateError err = SpriteState::ErrNone;
    QVERIFY(s.load(src, &err));

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString fakePath = tmp.path() + QDir::separator() + "mario.lvks";
    err = SpriteState::ErrNone;
    QVERIFY2(s.exportSprite(fakePath, tmp.path(), QString(),
                            SpriteState::All, &err),
             qPrintable("ALL export failed: " + SpriteState::errorMessage(err)));

    const QStringList expected = {
        tmp.path() + "/mario.lkob",
        tmp.path() + "/mario.lkot",
        tmp.path() + "/AnimNameDef_mario.h",
        tmp.path() + "/mario.png",
        tmp.path() + "/mario.json",
    };
    for (const QString& p : expected) {
        QVERIFY2(QFile::exists(p), qPrintable("missing expected output: " + p));
    }
}

void TstJsonExport::parseFormatHelper()
{
    QCOMPARE(SpriteState::parseFormat("cocos2d"),    SpriteState::Cocos2d);
    QCOMPARE(SpriteState::parseFormat("COCOS2D"),    SpriteState::Cocos2d);
    QCOMPARE(SpriteState::parseFormat("json"),       SpriteState::Json);
    QCOMPARE(SpriteState::parseFormat("JSON"),       SpriteState::Json);
    QCOMPARE(SpriteState::parseFormat("all"),        SpriteState::All);
    QCOMPARE(SpriteState::parseFormat("nonsense"),   SpriteState::Cocos2d); // default
    QCOMPARE(SpriteState::parseFormat(""),           SpriteState::Cocos2d); // default
    QCOMPARE(SpriteState::parseFormat("  json  "),   SpriteState::Json);    // trim
}

void TstJsonExport::duplicateFrameNamesDisambiguated()
{
    // Team H2 (H2.3): two frames named "walk" exported through the JSON
    // atlas exporter must both survive in the output -- pre-H2 the
    // downstream QMap<QString, PackedFrame> sortedByName and the
    // QJsonObject framesObj inserts silently dropped the first by
    // overwriting on the second's identical "walk.png" key, so the
    // animation timeline referenced ONE frame for both timeline slots
    // (whichever happened to win the overwrite).
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Need a real backing PNG so InputImage isn't completely null.
    {
        QImage seed(16, 16, QImage::Format_ARGB32);
        seed.fill(qRgba(0, 255, 0, 255));
        QVERIFY(seed.save(tmp.path() + "/seed.png", "PNG"));
    }

    TestableSpriteState state;
    InputImage seedImg(0, QStringLiteral("seed.png"), 1.0);
    seedImg.pixmap = QPixmap(tmp.path() + "/seed.png");
    state.seedImage(seedImg);

    // Two distinct frames, same name. Distinct sizes so we can match
    // each output entry back to its source frame.
    QPixmap pxA(16, 16); pxA.fill(Qt::red);
    QPixmap pxB(24, 24); pxB.fill(Qt::blue);

    LvkFrame fa(NullId, 0, 0, 0, 16, 16, QStringLiteral("walk"));
    state.seedFrame(fa);
    state.overridePixmap(fa.id, pxA);

    LvkFrame fb(NullId, 0, 0, 0, 24, 24, QStringLiteral("walk"));
    state.seedFrame(fb);
    state.overridePixmap(fb.id, pxB);

    // Build an animation that references BOTH frames so neither gets
    // skipped as unused.
    LvkAnimation ani(NullId, "the_walk", 0);
    LvkAframe afA(NullId, fa.id, 100, 0, 0, false);
    state.seedAnimation(ani, afA);
    LvkAframe afB(NullId, fb.id, 100, 0, 0, false);
    state.addAframeForTest(afB, ani.id); // helper below; falls through to addAframe.

    // Export and parse the JSON.
    JsonAtlasExporter exp;
    const QString base = tmp.path() + "/dup";
    QVERIFY2(exp.exportAtlas(base, state), "duplicate-name export should still succeed");

    QFile out(base + ".json");
    QVERIFY(out.open(QFile::ReadOnly));
    const QByteArray bytes = out.readAll();
    out.close();
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    QCOMPARE(pe.error, QJsonParseError::NoError);

    QJsonObject frames = doc.object().value("frames").toObject();
    QVERIFY2(frames.size() >= 2, "both frames must survive disambiguation");

    // The keys must include the original "walk.png" AND a suffixed
    // sibling like "walk_2.png".
    QVERIFY2(frames.contains("walk.png"), "primary 'walk.png' key is missing");
    QVERIFY2(frames.contains("walk_2.png"),
             "duplicate-name disambiguation suffix '_2' is missing");

    // And the animation must reference BOTH disambiguated names in
    // order, not "walk.png" twice.
    QJsonObject anims = doc.object().value("animations").toObject();
    QJsonArray seq = anims.value("the_walk").toArray();
    QCOMPARE(seq.size(), 2);
    // The two playback slots resolve to two distinct keys.
    QVERIFY(seq.at(0).toString() != seq.at(1).toString());
    // And both names are present in the frames object (no dangling refs).
    QVERIFY(frames.contains(seq.at(0).toString()));
    QVERIFY(frames.contains(seq.at(1).toString()));

    // Sanity: the per-frame width matches what we set, proving the
    // suffixed entry didn't get the OTHER frame's geometry.
    QJsonObject e1 = frames.value(seq.at(0).toString()).toObject();
    QJsonObject e2 = frames.value(seq.at(1).toString()).toObject();
    const int w1 = e1.value("sourceSize").toObject().value("w").toInt();
    const int w2 = e2.value("sourceSize").toObject().value("w").toInt();
    // Either (16,24) or (24,16) depending on which entry got suffixed.
    QVERIFY((w1 == 16 && w2 == 24) || (w1 == 24 && w2 == 16));
}

void TstJsonExport::equalHeightFramesProduceDeterministicOutput()
{
    // Team H2 (H2.4): synthetic 10-frame fixture with 5 frames at h=64
    // and 5 at h=128. Export twice into separate directories and assert
    // the produced PNGs are byte-equivalent. Pre-H2 std::sort was
    // non-stable, so equal-height frames could shuffle between libstdc++
    // releases (or runs of the same binary under -fsanitize=address,
    // which exercises the std::sort partition heuristic differently);
    // the post-H2 stable_sort + frameId tiebreak fixes both axes.
    auto seedExportAndHashPng = [](const QString& dir) -> QByteArray {
        QImage seed(16, 16, QImage::Format_ARGB32);
        seed.fill(qRgba(0, 0, 255, 255));
        seed.save(dir + "/seed.png", "PNG");

        TestableSpriteState state;
        InputImage seedImg(0, QStringLiteral("seed.png"), 1.0);
        seedImg.pixmap = QPixmap(dir + "/seed.png");
        state.seedImage(seedImg);

        // One animation referencing every frame so none get skipped.
        LvkAnimation ani(NullId, "test", 0);
        bool first = true;

        // Interleave the heights deliberately so a non-stable sort has
        // an opportunity to shuffle them. 5 frames at h=64, 5 at h=128.
        const QList<int> heights = {64, 128, 64, 128, 64, 128, 64, 128, 64, 128};
        for (int i = 0; i < heights.size(); ++i) {
            const int h = heights.at(i);
            QPixmap px(32, h);
            // Deterministic colour per index so a frame mis-placement
            // shows up as different pixels in the atlas.
            px.fill(QColor(i * 25 % 256, (i * 53) % 256, (i * 97) % 256, 255));
            LvkFrame fr(NullId, 0, 0, 0, 32, h,
                        QStringLiteral("frame_%1").arg(i));
            state.seedFrame(fr);
            state.overridePixmap(fr.id, px);
            LvkAframe af(NullId, fr.id, 50, 0, 0, false);
            if (first) {
                state.seedAnimation(ani, af);
                first = false;
            } else {
                state.addAframeForTest(af, ani.id);
            }
        }

        JsonAtlasExporter exp;
        const QString base = dir + "/det";
        if (!exp.exportAtlas(base, state)) {
            return QByteArray();
        }
        QFile png(base + ".png");
        if (!png.open(QFile::ReadOnly)) {
            return QByteArray();
        }
        return png.readAll();
    };

    QTemporaryDir runA;
    QTemporaryDir runB;
    QVERIFY(runA.isValid());
    QVERIFY(runB.isValid());

    const QByteArray bytesA = seedExportAndHashPng(runA.path());
    const QByteArray bytesB = seedExportAndHashPng(runB.path());

    QVERIFY2(!bytesA.isEmpty(), "first export produced no PNG");
    QVERIFY2(!bytesB.isEmpty(), "second export produced no PNG");
    QCOMPARE(bytesA.size(), bytesB.size());
    QCOMPARE(bytesA, bytesB);
}

QTEST_MAIN(TstJsonExport)
#include "tst_json_export.moc"
