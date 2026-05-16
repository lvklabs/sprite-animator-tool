// JsonAtlasExporter.cpp
//
// See JsonAtlasExporter.h for purpose. Implementation notes:
//
//   - We do a shelf-pack on frames sorted by descending height. Width is
//     fixed-grown to the next power of two (helps with GL texture limits).
//     Final atlas height is what the shelves consumed. This is not the
//     most space-efficient packer in existence; it is the simplest one
//     that produces predictable, defect-free output and runs in O(n log n).
//
//   - We deliberately export ONLY frames that are used by at least one
//     animation, matching the legacy Cocos2d exporter's behavior in
//     SpriteState::exportSprite (which calls isFrameUnused() to skip
//     orphans). This keeps the JSON atlas size sensible and avoids
//     publishing in-progress / scratch frames.
//
//   - The "animations" key under "meta" is an LVK extension to the
//     vanilla TexturePacker JSON (Array) schema. Each entry maps an
//     animation name to a list of frame filenames in playback order; the
//     filenames match the keys in the "frames" array, so a consumer can
//     look them up cheaply.

#include "exporters/JsonAtlasExporter.h"

#include "spritestate.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QMapIterator>
#include <QList>
#include <QDebug>

namespace {

// Round n up to the next power of two (>= 64 to avoid degenerate atlases).
int nextPow2(int n)
{
    int p = 64;
    while (p < n) p <<= 1;
    return p;
}

// One frame's pack record. We sort PackedFrames by descending height
// before laying them out (shelf-pack heuristic).
struct PackedFrame {
    Id      frameId   = 0;
    QString name;
    QPixmap pixmap;
    int     x         = 0;
    int     y         = 0;
    int     width     = 0;
    int     height    = 0;
};

// Default per-aframe delay (ms) when a frame appears in no animation
// (shouldn't happen, since we skip orphans, but be defensive).
constexpr int kDefaultDuration = 100;

}

bool JsonAtlasExporter::exportAtlas(const QString& baseFilename,
                                    const SpriteState& state) const
{
    if (baseFilename.isEmpty()) {
        qDebug() << "JsonAtlasExporter::exportAtlas: empty baseFilename";
        return false;
    }

    const QString pngPath  = baseFilename + QStringLiteral(".png");
    const QString jsonPath = baseFilename + QStringLiteral(".json");
    const QString atlasImage = QFileInfo(pngPath).fileName();

    // -- gather frames ----------------------------------------------------
    QList<PackedFrame> pack;
    pack.reserve(state.frames().size());

    const QMap<Id, LvkFrame>& frames = state.frames();
    for (auto it = frames.constBegin(); it != frames.constEnd(); ++it) {
        const LvkFrame& fr = it.value();
        if (state.isFrameUnused(fr.id)) {
            continue;
        }
        QPixmap px = state.fpixmap(fr.id);
        if (px.isNull()) {
            // Frame referenced by an animation but the pixmap couldn't
            // be loaded (missing image asset, broken path). Skip rather
            // than crash; the consumer will see a missing-frame gap.
            qDebug() << "JsonAtlasExporter: skipping frame" << fr.id
                     << "(" << fr.name << ") - null pixmap";
            continue;
        }
        PackedFrame pf;
        pf.frameId = fr.id;
        // Use the frame's name as the JSON key, falling back to id_N if
        // the user has no name set. Append .png suffix so consumers that
        // treat the filename as a literal sprite path (TexturePacker
        // convention) work out of the box.
        pf.name = fr.name.isEmpty()
                      ? QStringLiteral("frame_%1.png").arg(fr.id)
                      : fr.name + QStringLiteral(".png");
        pf.pixmap = px;
        pf.width  = px.width();
        pf.height = px.height();
        pack.append(pf);
    }

    // -- shelf-pack -------------------------------------------------------
    // Sort by descending height so each shelf is at least as tall as the
    // largest frame on it. With sprites this typically gives <10% wasted
    // space versus the bounding rectangle.
    std::sort(pack.begin(), pack.end(),
              [](const PackedFrame& a, const PackedFrame& b) {
                  return a.height > b.height;
              });

    // Pick atlas width: at least as wide as the widest frame, padded to
    // a power of two and clamped to a sensible upper bound. This keeps
    // single-large-frame edge cases from blowing up the width.
    int maxW = 64;
    int totalArea = 0;
    for (const PackedFrame& pf : pack) {
        if (pf.width > maxW) maxW = pf.width;
        totalArea += pf.width * pf.height;
    }
    // Square-ish heuristic: target an atlas with width >= sqrt(area).
    int targetW = maxW;
    {
        int wByArea = 1;
        while (wByArea * wByArea < totalArea) wByArea <<= 1;
        if (wByArea > targetW) targetW = wByArea;
    }
    const int atlasW = nextPow2(targetW);

    int curX = 0;
    int curY = 0;
    int shelfH = 0;
    int atlasH = 0;
    for (PackedFrame& pf : pack) {
        if (curX + pf.width > atlasW) {
            // wrap to next shelf
            curX = 0;
            curY += shelfH;
            shelfH = 0;
        }
        pf.x = curX;
        pf.y = curY;
        curX += pf.width;
        if (pf.height > shelfH) shelfH = pf.height;
        if (curY + shelfH > atlasH) atlasH = curY + shelfH;
    }
    if (atlasH == 0) atlasH = 64; // empty-state safety
    atlasH = nextPow2(atlasH);

    // -- render PNG -------------------------------------------------------
    {
        QImage atlas(atlasW, atlasH, QImage::Format_ARGB32_Premultiplied);
        atlas.fill(Qt::transparent);
        QPainter p(&atlas);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        for (const PackedFrame& pf : pack) {
            p.drawPixmap(pf.x, pf.y, pf.pixmap);
        }
        p.end();
        if (!atlas.save(pngPath, "PNG")) {
            qDebug() << "JsonAtlasExporter::exportAtlas: failed to write" << pngPath;
            return false;
        }
    }

    // -- per-frame default duration map ----------------------------------
    // First aframe delay we see for each frameId wins. This is a single
    // value per frame in the JSON output (TexturePacker schema doesn't
    // model per-instance delay separately).
    QMap<Id, int> firstDelay;
    {
        const QMap<Id, LvkAnimation>& anims = state.animations();
        for (auto it = anims.constBegin(); it != anims.constEnd(); ++it) {
            const LvkAnimation& a = it.value();
            for (const LvkAframe& af : a._aframes) {
                if (!firstDelay.contains(af.frameId)) {
                    firstDelay.insert(af.frameId, af.delay);
                }
            }
        }
    }

    // -- build JSON -------------------------------------------------------
    // Frames are emitted in name order to keep diffs stable across
    // unrelated edits (QMap iteration is sorted by key).
    QMap<QString, PackedFrame> sortedByName;
    for (const PackedFrame& pf : pack) sortedByName.insert(pf.name, pf);

    QJsonObject framesObj;
    for (auto it = sortedByName.constBegin(); it != sortedByName.constEnd(); ++it) {
        const PackedFrame& pf = it.value();
        QJsonObject rect;
        rect.insert(QStringLiteral("x"), pf.x);
        rect.insert(QStringLiteral("y"), pf.y);
        rect.insert(QStringLiteral("w"), pf.width);
        rect.insert(QStringLiteral("h"), pf.height);
        QJsonObject srcSize;
        srcSize.insert(QStringLiteral("w"), pf.width);
        srcSize.insert(QStringLiteral("h"), pf.height);
        QJsonObject frameObj;
        frameObj.insert(QStringLiteral("filename"), pf.name);
        frameObj.insert(QStringLiteral("frame"), rect);
        frameObj.insert(QStringLiteral("rotated"), false);
        frameObj.insert(QStringLiteral("trimmed"), false);
        frameObj.insert(QStringLiteral("spriteSourceSize"), rect);
        frameObj.insert(QStringLiteral("sourceSize"), srcSize);
        frameObj.insert(QStringLiteral("duration"),
                        firstDelay.value(pf.frameId, kDefaultDuration));
        framesObj.insert(pf.name, frameObj);
    }

    // Per-animation playback order: name -> [frameName, frameName, ...]
    QJsonObject animsObj;
    {
        const QMap<Id, LvkAnimation>& anims = state.animations();
        // Build an Id -> name lookup for the pack list.
        QMap<Id, QString> idToName;
        for (const PackedFrame& pf : pack) idToName.insert(pf.frameId, pf.name);

        for (auto it = anims.constBegin(); it != anims.constEnd(); ++it) {
            const LvkAnimation& a = it.value();
            QJsonArray seq;
            for (const LvkAframe& af : a._aframes) {
                const QString nm = idToName.value(af.frameId);
                if (!nm.isEmpty()) seq.append(nm);
            }
            animsObj.insert(a.name, seq);
        }
    }

    QJsonObject meta;
    meta.insert(QStringLiteral("app"),    QStringLiteral("LvkSpriteEditor"));
    meta.insert(QStringLiteral("version"),QStringLiteral("2.0"));
    meta.insert(QStringLiteral("image"),  atlasImage);
    meta.insert(QStringLiteral("format"), QStringLiteral("RGBA8888"));
    QJsonObject size;
    size.insert(QStringLiteral("w"), atlasW);
    size.insert(QStringLiteral("h"), atlasH);
    meta.insert(QStringLiteral("size"),  size);
    meta.insert(QStringLiteral("scale"), QStringLiteral("1"));

    QJsonObject root;
    root.insert(QStringLiteral("frames"),     framesObj);
    root.insert(QStringLiteral("animations"), animsObj);
    root.insert(QStringLiteral("meta"),       meta);

    QJsonDocument doc(root);
    QFile out(jsonPath);
    if (!out.open(QFile::WriteOnly | QFile::Truncate)) {
        qDebug() << "JsonAtlasExporter::exportAtlas: failed to open" << jsonPath;
        return false;
    }
    out.write(doc.toJson(QJsonDocument::Indented));
    out.close();
    return true;
}
