// JsonAtlasExporter.h
//
// Generic sprite-atlas exporter (Agent 8/10, Phase 3). Produces a
// TexturePacker-compatible "JSON (Array)" descriptor + a packed PNG
// sprite-sheet from a SpriteState. Designed to be the JSON counterpart
// to SpriteState::exportSprite()'s Cocos2d pipeline.
//
// Output:
//   - <base>.png  - single image containing every exported frame laid
//                   out via a simple shelf-pack algorithm.
//   - <base>.json - top-level "meta" + "frames" object + "animations"
//                   field. The "animations" key is an LVK-specific
//                   extension to the TexturePacker schema since vanilla
//                   TexturePacker does not store sequence information.

#ifndef LVK_EXPORTERS_JSON_ATLAS_EXPORTER_H
#define LVK_EXPORTERS_JSON_ATLAS_EXPORTER_H

#include <QString>

class SpriteState;

class JsonAtlasExporter
{
public:
    JsonAtlasExporter() = default;

    /// Export <baseFilename>.png + <baseFilename>.json.
    /// @param baseFilename absolute path WITHOUT extension; the exporter
    ///        appends ".png" and ".json".
    /// @param state source sprite data; we read frames(), animations(),
    ///        fpixmaps() and isFrameUnused().
    /// Returns true on success.
    bool exportAtlas(const QString& baseFilename, const SpriteState& state) const;
};

#endif // LVK_EXPORTERS_JSON_ATLAS_EXPORTER_H
