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
        const QString baseStem =
            fr.name.isEmpty() ? QStringLiteral("frame_%1").arg(fr.id) : fr.name;
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
    std::stable_sort(pack.begin(), pack.end(), [](const PackedFrame &a, const PackedFrame &b) {
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

    // Refuse to pack if the REQUIRED extent exceeds the dimension cap.
    // atlasH here is the exact shelf-packed height (max curY + shelfH), so
    // comparing it against the cap accounts for end-of-shelf waste; a
    // combined-area heuristic does not (frames can pass an area check yet
    // still need more shelves than the clamped height holds, which would
    // silently clip them out of the PNG while the JSON records
    // out-of-bounds coordinates). Same for a single frame wider than the
    // cap: it could never fit a shelf.
    if (atlasH > kMaxAtlasDim || maxW > kMaxAtlasDim) {
        qDebug() << "JsonAtlasExporter::exportAtlas: refusing to pack frames: required"
                 << "atlas extent" << maxW << "x" << atlasH << "exceeds dimension cap"
                 << kMaxAtlasDim;
        return false;
    }
    atlasH = nextPow2(atlasH);
    atlasH = qMin(atlasH, kMaxAtlasDim);

    // -- render PNG -------------------------------------------------------
    // J4.5: write to "<pngPath>.tmp" first; promote with QFile::rename
    // only AFTER the JSON has been successfully written via QSaveFile.
    // This ensures that if the JSON write fails, the PRIOR (pngPath,
    // jsonPath) pair (if any) is left completely untouched -- the
    // previous behaviour overwrote pngPath unconditionally and then,
    // on JSON failure, removed it, leaving any pre-existing jsonPath
    // referencing a now-missing PNG.
    const QString pngTmpPath = pngPath + QStringLiteral(".tmp");
    {
        QImage atlas(atlasW, atlasH, QImage::Format_ARGB32_Premultiplied);
        atlas.fill(Qt::transparent);
        QPainter p(&atlas);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        for (const PackedFrame &pf : pack) {
            p.drawPixmap(pf.x, pf.y, pf.pixmap);
        }
        p.end();
        // Defensive: clear any leftover tmp from a previous failed run so
        // QImage::save sees a clean slate.
        QFile::remove(pngTmpPath);
        if (!atlas.save(pngTmpPath, "PNG")) {
            qDebug() << "JsonAtlasExporter::exportAtlas: failed to write" << pngTmpPath;
            QFile::remove(pngTmpPath);
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

    // J4.5: atomic-pair publish.
    //
    // The new PNG sits in pngTmpPath. The JSON is staged through
    // QSaveFile (which writes to its own ".XXXXXX" temp and only
    // installs at commit()). Until BOTH writes have succeeded, neither
    // pngPath nor jsonPath is touched -- so a pre-existing (PNG, JSON)
    // pair survives any failure here completely intact.
    //
    // The legacy "PNG first, JSON second, remove PNG on JSON failure"
    // strategy could not honour that invariant: if a prior export had
    // already written pngPath / jsonPath, the PNG write would clobber
    // pngPath, and a subsequent JSON failure removed the new PNG --
    // but the prior jsonPath remained, now pointing at a deleted
    // sibling. Now both files are promoted together (or rolled back
    // together).
    QJsonDocument doc(root);
    QSaveFile out(jsonPath);
    out.setDirectWriteFallback(true);
    if (!out.open(QFile::WriteOnly | QFile::Truncate)) {
        qDebug() << "JsonAtlasExporter::exportAtlas: failed to open" << jsonPath << "-"
                 << out.errorString();
        QFile::remove(pngTmpPath); // prior pngPath / jsonPath untouched
        return false;
    }
    const QByteArray jsonBytes = doc.toJson(QJsonDocument::Indented);
    if (out.write(jsonBytes) != jsonBytes.size()) {
        qDebug() << "JsonAtlasExporter::exportAtlas: short write on" << jsonPath << "-"
                 << out.errorString();
        out.cancelWriting();
        out.commit(); // discards QSaveFile's internal tmp; jsonPath untouched
        QFile::remove(pngTmpPath);
        return false;
    }
    // Both writes successful in their temp locations. Promote.
    //
    // QSaveFile::commit() is a single rename(2) on POSIX which atomically
    // installs the JSON. We do that FIRST so a commit failure leaves the
    // prior pair intact: the new PNG is still only in pngTmpPath, and we
    // can roll it back by removing pngTmpPath.
    if (!out.commit()) {
        qDebug() << "JsonAtlasExporter::exportAtlas: commit() failed for" << jsonPath << "-"
                 << out.errorString();
        QFile::remove(pngTmpPath); // prior pngPath / jsonPath untouched
        return false;
    }
    // JSON has landed. Now promote the PNG. If a prior pngPath exists,
    // QFile::rename will refuse to overwrite, so we remove it first.
    // (POSIX rename(2) is atomic; the prior PNG is gone for a few syscalls
    // until QFile::rename installs the new one.) The window where pngPath
    // is briefly absent is the unavoidable cost of QFile's no-overwrite
    // policy. On the unlikely rename failure we leave a stale jsonPath +
    // pngTmpPath sidecar and warn -- the JSON has already been committed
    // so we cannot roll back to "prior pair" without overwriting jsonPath
    // a second time, which itself can fail.
    QFile::remove(pngPath);
    if (!QFile::rename(pngTmpPath, pngPath)) {
        qDebug() << "JsonAtlasExporter::exportAtlas: failed to promote" << pngTmpPath << "to"
                 << pngPath << "- JSON written but PNG not in place; leaving" << pngTmpPath
                 << "behind for manual recovery";
        return false;
    }
    return true;
}
