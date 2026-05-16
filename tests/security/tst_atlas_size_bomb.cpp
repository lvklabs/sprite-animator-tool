// tests/security/tst_atlas_size_bomb.cpp
//
// Phase 4 (Item 16): bound frame dimensions and the atlas allocation.
//
// Two attack shapes:
//   (a) A single frame with bogus w/h fed via LvkFrame::fromString.
//       INT_MAX-class values silently overflowed totalArea, made the
//       packer pick a >GB atlas, and could OOM the process even before
//       QImage allocation.
//   (b) Thousands of legitimate-sized frames whose combined area would
//       exceed the 16384x16384 clamped atlas. JsonAtlasExporter must
//       refuse rather than truncate the layout.
//
// The exporter is exercised end-to-end through SpriteState because that
// is the only public surface; we seed it via a Testable subclass that
// reaches in through addImage/addFrame.

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPixmap>
#include <QTemporaryDir>

#include "exporters/JsonAtlasExporter.h"
#include "spritestate.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

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
    // Inject a synthesised pixmap directly into _fpixmaps without going
    // through reloadFramePixmap (whose QPixmap::copy would clip to the
    // source image bounds). The exporter only reads _fpixmaps so this
    // is sufficient for a synthetic-large-frame test.
    void overridePixmap(Id frameId, const QPixmap& px) {
        _fpixmaps[frameId] = px;
    }
};


class TstAtlasSizeBomb : public QObject
{
    Q_OBJECT
private slots:
    void rejectsOversizedFrameInFromString();
    void rejectsAtlasAreaOverflow();
};


void TstAtlasSizeBomb::rejectsOversizedFrameInFromString()
{
    // Item 16 sub-test 1: w near INT_MAX must be rejected up front.
    LvkFrame f;
    const bool ok = f.fromString(QStringLiteral("0,name,0,0,0,2000000000,1"));
    QVERIFY2(!ok, "LvkFrame::fromString accepted w=2_000_000_000");
    QCOMPARE(f.w, 0);

    // Also reject a slightly-out-of-bounds dim (8193) to nail down the
    // 8192 cap.
    LvkFrame f2;
    const bool ok2 = f2.fromString(QStringLiteral("0,name,0,0,0,8193,8193"));
    QVERIFY2(!ok2, "LvkFrame::fromString accepted 8193x8193");

    // Sanity: a normal 256x256 frame must still succeed.
    LvkFrame f3;
    const bool ok3 = f3.fromString(QStringLiteral("5,sprite,2,1,2,256,256"));
    QVERIFY2(ok3, "LvkFrame::fromString rejected a legitimate 256x256 frame");
    QCOMPARE(f3.id, 5);
    QCOMPARE(f3.w, 256);
    QCOMPARE(f3.h, 256);
}


void TstAtlasSizeBomb::rejectsAtlasAreaOverflow()
{
    // Item 16 sub-test 2: 1000 frames of 4096x4096 each. Combined area
    // is 1000 * 4096*4096 = 1.6e10 pixels, dwarfing the 16384*16384 ==
    // 2.68e8 cap. JsonAtlasExporter must refuse the export.
    QTemporaryDir workdir;
    QVERIFY(workdir.isValid());
    QDir::setCurrent(workdir.path());

    // Need a backing image so the InputImage isn't completely null.
    {
        QImage seed(16, 16, QImage::Format_ARGB32);
        seed.fill(qRgba(0, 0, 255, 255));
        QVERIFY(seed.save("seed.png", "PNG"));
    }

    TestableSpriteState state;
    InputImage seedImg(0, QStringLiteral("seed.png"), 1.0);
    seedImg.pixmap = QPixmap("seed.png");
    state.seedImage(seedImg);

    // Build a 4096x4096 transparent pixmap once and re-use it across
    // every frame; the exporter only reads dimensions per pf, but the
    // image is needed so the pack record isn't skipped.
    QPixmap big(4096, 4096);
    big.fill(Qt::transparent);

    LvkAnimation ani(NullId, "bomb_ani", 0);
    LvkAframe firstAf(NullId, 0, 50, 0, 0, false);
    state.seedAnimation(ani, firstAf);
    // (The seed call above doesn't matter for this test; we just need
    // an animation so the bomb frames are not "unused" and get packed.)

    for (int i = 0; i < 1000; ++i) {
        LvkFrame fr(NullId, 0, 0, 0, 4096, 4096,
                    QStringLiteral("bomb_%1").arg(i));
        state.seedFrame(fr);
        state.overridePixmap(fr.id, big);
        LvkAframe af(NullId, fr.id, 50, 0, 0, false);
        // We need at least one animation referencing each frame, else
        // the exporter skips them (isFrameUnused()).
        LvkAnimation bombAni(NullId, QStringLiteral("ani_%1").arg(i), 0);
        state.seedAnimation(bombAni, af);
    }

    JsonAtlasExporter exp;
    const QString base = workdir.path() + QDir::separator() + "atlas_bomb";
    const bool ok = exp.exportAtlas(base, state);
    QVERIFY2(!ok, "JsonAtlasExporter accepted a 1000x4096x4096 area bomb. "
                  "totalArea exceeds the clamped atlas area but the "
                  "exporter did not bail.");
    QVERIFY2(!QFile::exists(base + QStringLiteral(".png")),
             "atlas_bomb.png written despite overflow refusal");
}


int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TstAtlasSizeBomb tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_atlas_size_bomb.moc"
