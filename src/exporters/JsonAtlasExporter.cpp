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

#include "lvkaframe.h"
#include "lvkanimation.h"
#include "lvkframe.h"
#include "spritestate.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QMapIterator>
#include <QPainter>
#include <QPixmap>
#include <QSaveFile>
#include <QSet>

namespace {

// Round n up to the next power of two (>= 64 to avoid degenerate atlases).
int nextPow2(int n) {
    int p = 64;
    while (p < n)
        p <<= 1;
    return p;
}

// One frame's pack record. We sort PackedFrames by descending height
// before laying them out (shelf-pack heuristic).
struct PackedFrame {
    Id frameId = 0;
    QString name;
    QPixmap pixmap;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Default per-aframe delay (ms) when a frame appears in no animation
// (shouldn't happen, since we skip orphans, but be defensive).
constexpr int kDefaultDuration = 100;

} // namespace

bool JsonAtlasExporter::exportAtlas(const QString &baseFilename, const SpriteState &state) const {
    if (baseFilename.isEmpty()) {
        qDebug() << "JsonAtlasExporter::exportAtlas: empty baseFilename";
        return false;
    }

    const QString pngPath = baseFilename + QStringLiteral(".png");
    const QString jsonPath = baseFilename + QStringLiteral(".json");
    const QString atlasImage = QFileInfo(pngPath).fileName();

    // -- gather frames ----------------------------------------------------
    QList<PackedFrame> pack;
    pack.reserve(state.frames().size());

    // Team H2 (H2.3): disambiguate duplicate frame names by appending a
    // numeric suffix. Before this, two frames both named "walk" would
    // both serialize as pf.name=="walk.png"; the downstream QMap<QString,
    // PackedFrame> and QJsonObject "frames" inserts then silently
    // overwrote each other -- the JSON ended up with ONE "walk.png"
    // entry holding whichever frame happened to be inserted last, and
    // any animation reference resolving through idToName (built from
    // pack[], so populated with both names) pointed at a key that no
    // longer existed. Net effect: walk animation played a wrong/missing
    // frame depending on insertion order.
    //
    // Fix: track names already assigned in the pack[] list and append
    // "_2", "_3", ... to collisions BEFORE the ".png" suffix so the
    // resulting JSON key is "walk_2.png". The frameId-keyed idToName
    // map further down uses the SUFFIXED name, so animation references
    // resolve to the right entry.
    QSet<QString> usedNames;
    const QMap<Id, LvkFrame> &frames = state.frames();
    for (auto it = frames.constBegin(); it != frames.constEnd(); ++it) {
        const LvkFrame &fr = it.value();
        if (state.isFrameUnused(fr.id)) {
            continue;
        }
        QPixmap px = state.fpixmap(fr.id);
        if (px.isNull()) {
            // Frame referenced by an animation but the pixmap couldn't
            // be loaded (missing image asset, broken path). Skip rather
            // than crash; the consumer will see a missing-frame gap.
            qDebug() << "JsonAtlasExporter: skipping frame" << fr.id << "(" << fr.name
                     << ") - null pixmap";
            continue;
        }
        PackedFrame pf;
        pf.frameId = fr.id;
        // Use the frame's name as the JSON key, falling back to id_N if
        // the user has no name set. Append .png suffix so consumers that
        // treat the filename as a literal sprite path (TexturePacker
        // convention) work out of the box.
        const QString baseStem = fr.name.isEmpty() ? QStringLiteral("frame_%1").arg(fr.id)
                                                   : fr.name;
        QString stem = baseStem;
        int suffix = 2;
        while (usedNames.contains(stem + QStringLiteral(".png"))) {
            stem = baseStem + QStringLiteral("_") + QString::number(suffix++);
        }
        pf.name = stem + QStringLiteral(".png");
        usedNames.insert(pf.name);
        pf.pixmap = px;
        pf.width = px.width();
        pf.height = px.height();
        pack.append(pf);
    }

    // -- shelf-pack -------------------------------------------------------
    // Sort by descending height so each shelf is at least as tall as the
    // largest frame on it. With sprites this typically gives <10% wasted
    // space versus the bounding rectangle.
    //
    // Team H2 (H2.4): use stable_sort with an explicit frameId tiebreak so
    // equal-height frames keep a deterministic order across libstdc++
    // versions, libc++ vs MSVC STLs, and across runs. The previous
    // std::sort was non-stable AND had no tiebreaker, so two libstdc++
    // releases could legitimately place equal-height frames at different
    // (x, y) shelf positions, producing byte-different atlas PNGs from
    // byte-identical input. That broke "diff the JSON to spot real
    // changes" workflows and any consumer doing content-hash dedup on
    // the atlas. stable_sort + explicit tiebreak is belt-and-braces:
    // the tiebreak alone makes the comparator a total order so even
    // std::sort would be deterministic, and stable_sort additionally
    // pins iteration-order ties (insertion order from the QMap loop
    // above) for free.
    std::stable_sort(pack.begin(), pack.end(),
                     [](const PackedFrame &a, const PackedFrame &b) {
                         if (a.height != b.height) {
                             return a.height > b.height;
                         }
                         return a.frameId < b.frameId;
                     });

    // SECURITY (Phase 4): Bound atlas allocations.
    //   - kMaxAtlasDim caps a single axis. 16384 matches the maximum
    //     texture size GPUs typically advertise, and keeps the ARGB32
    //     atlas allocation under 1 GB even at the worst-case square.
    //   - totalArea is qint64; multiplication is widened explicitly to
    //     avoid int*int -> int overflow at w*h >= 2^31 (e.g. one frame
    //     of 65536x65536 is enough -- and a malicious .lvks could
    //     before the lvkframe.cpp bounds check landed).
    constexpr int kMaxAtlasDim = 16384;

    // Pick atlas width: at least as wide as the widest frame, padded to
    // a power of two and clamped to a sensible upper bound. This keeps
    // single-large-frame edge cases from blowing up the width.
    int maxW = 64;
    qint64 totalArea = 0;
    for (const PackedFrame &pf : pack) {
        if (pf.width > maxW)
            maxW = pf.width;
        totalArea += static_cast<qint64>(pf.width) * static_cast<qint64>(pf.height);
    }
    // Square-ish heuristic: target an atlas with width >= sqrt(area).
    int targetW = maxW;
    {
        qint64 wByArea = 1;
        while (wByArea * wByArea < totalArea)
            wByArea <<= 1;
        if (wByArea > targetW)
            targetW = static_cast<int>(qMin<qint64>(wByArea, kMaxAtlasDim));
    }
    int atlasW = nextPow2(targetW);
    atlasW = qMin(atlasW, kMaxAtlasDim);

    int curX = 0;
    int curY = 0;
    int shelfH = 0;
    int atlasH = 0;
    for (PackedFrame &pf : pack) {
        if (curX + pf.width > atlasW) {
            // wrap to next shelf
            curX = 0;
            curY += shelfH;
            shelfH = 0;
        }
        pf.x = curX;
        pf.y = curY;
        curX += pf.width;
        if (pf.height > shelfH)
            shelfH = pf.height;
        if (curY + shelfH > atlasH)
            atlasH = curY + shelfH;
    }
    if (atlasH == 0)
        atlasH = 64; // empty-state safety
    atlasH = nextPow2(atlasH);
    atlasH = qMin(atlasH, kMaxAtlasDim);

    // After clamping, if the input frames could not possibly fit inside
    // the clamped atlas (their combined area exceeds the clamped square),
    // bail with an error rather than truncating frame data into a
    // too-small canvas.
    const qint64 clampedAtlasArea = static_cast<qint64>(atlasW) * static_cast<qint64>(atlasH);
    if (totalArea > clampedAtlasArea) {
        qDebug() << "JsonAtlasExporter::exportAtlas: refusing to pack frames "
                 << "totalArea=" << totalArea << "into atlas of clamped area=" << clampedAtlasArea
                 << "(dim cap" << kMaxAtlasDim << ")";
        return false;
    }

    // -- render PNG -------------------------------------------------------
    {
        QImage atlas(atlasW, atlasH, QImage::Format_ARGB32_Premultiplied);
        atlas.fill(Qt::transparent);
        QPainter p(&atlas);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        for (const PackedFrame &pf : pack) {
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
        const QMap<Id, LvkAnimation> &anims = state.animations();
        for (auto it = anims.constBegin(); it != anims.constEnd(); ++it) {
            const LvkAnimation &a = it.value();
            for (const LvkAframe &af : a._aframes) {
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
    for (const PackedFrame &pf : pack)
        sortedByName.insert(pf.name, pf);

    QJsonObject framesObj;
    for (auto it = sortedByName.constBegin(); it != sortedByName.constEnd(); ++it) {
        const PackedFrame &pf = it.value();
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
        frameObj.insert(QStringLiteral("duration"), firstDelay.value(pf.frameId, kDefaultDuration));
        framesObj.insert(pf.name, frameObj);
    }

    // Per-animation playback order: name -> [frameName, frameName, ...]
    QJsonObject animsObj;
    {
        const QMap<Id, LvkAnimation> &anims = state.animations();
        // Build an Id -> name lookup for the pack list.
        QMap<Id, QString> idToName;
        for (const PackedFrame &pf : pack)
            idToName.insert(pf.frameId, pf.name);

        for (auto it = anims.constBegin(); it != anims.constEnd(); ++it) {
            const LvkAnimation &a = it.value();
            QJsonArray seq;
            for (const LvkAframe &af : a._aframes) {
                const QString nm = idToName.value(af.frameId);
                if (!nm.isEmpty())
                    seq.append(nm);
            }
            animsObj.insert(a.name, seq);
        }
    }

    QJsonObject meta;
    meta.insert(QStringLiteral("app"), QStringLiteral("LvkSpriteEditor"));
    meta.insert(QStringLiteral("version"), QStringLiteral("2.0"));
    meta.insert(QStringLiteral("image"), atlasImage);
    meta.insert(QStringLiteral("format"), QStringLiteral("RGBA8888"));
    QJsonObject size;
    size.insert(QStringLiteral("w"), atlasW);
    size.insert(QStringLiteral("h"), atlasH);
    meta.insert(QStringLiteral("size"), size);
    meta.insert(QStringLiteral("scale"), QStringLiteral("1"));

    QJsonObject root;
    root.insert(QStringLiteral("frames"), framesObj);
    root.insert(QStringLiteral("animations"), animsObj);
    root.insert(QStringLiteral("meta"), meta);

    // Team H2 (H2.5): atomic best-effort for the JSON + PNG pair.
    //
    // Pre-H2 the sequence was:
    //   1. atlas.save(pngPath, "PNG")     -- regular open+write+close
    //   2. QFile out(jsonPath); ... write ... close()
    // A crash, OOM, or full disk between step 1 and step 2 left the
    // PNG on disk WITHOUT a matching JSON descriptor. Consumers that
    // probe for both files would see a half-built export and either
    // crash or silently render garbage.
    //
    // Strategy (the "simpler alternative" called out in the task
    // brief): keep "PNG first, JSON second" but use QSaveFile for the
    // JSON write so the publish is atomic on the JSON side, and on a
    // JSON failure ALSO remove the PNG so the on-disk state is
    // "either both files or neither". Qt's QImageWriter has no
    // QSaveFile-equivalent, so the PNG side is best-effort: a crash
    // mid-PNG-write may leave a partial PNG, but in that case the
    // JSON has not yet been written either, so the "neither file"
    // invariant still holds modulo the partial-PNG corruption (no
    // matching descriptor -> consumer treats it as absent). The
    // window where both files are present and inconsistent shrinks
    // from "between two normal open/write/close calls" to "between
    // PNG-close and QSaveFile::commit() of the JSON" -- a single
    // syscall on POSIX.
    QJsonDocument doc(root);
    QSaveFile out(jsonPath);
    out.setDirectWriteFallback(true);
    if (!out.open(QFile::WriteOnly | QFile::Truncate)) {
        qDebug() << "JsonAtlasExporter::exportAtlas: failed to open" << jsonPath
                 << "-" << out.errorString();
        // Roll back the orphan PNG so the (PNG, JSON) pair is "neither".
        QFile::remove(pngPath);
        return false;
    }
    const QByteArray jsonBytes = doc.toJson(QJsonDocument::Indented);
    if (out.write(jsonBytes) != jsonBytes.size()) {
        qDebug() << "JsonAtlasExporter::exportAtlas: short write on" << jsonPath
                 << "-" << out.errorString();
        out.cancelWriting();
        out.commit(); // remove tmp; leaves jsonPath untouched
        QFile::remove(pngPath);
        return false;
    }
    if (!out.commit()) {
        qDebug() << "JsonAtlasExporter::exportAtlas: commit() failed for" << jsonPath
                 << "-" << out.errorString();
        QFile::remove(pngPath);
        return false;
    }
    return true;
}
