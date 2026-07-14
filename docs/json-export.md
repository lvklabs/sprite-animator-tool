# JSON sprite-atlas export (`--format json`)

The JSON exporter (`src/exporters/JsonAtlasExporter.{h,cpp}`) writes a
**packed sprite sheet** plus a **TexturePacker-compatible JSON
descriptor** for engines that don't consume the Cocos2d
`.lkob`/`.lkot`/`.h` trio (see [`cocos2d-export.md`](cocos2d-export.md)
for that pipeline).

Selected with:

- CLI: `LvkSpriteEditor --export sprite.lvks --format json` (or
  `--format all` for both pipelines),
- GUI: **File → Export as...** with the *JSON Atlas* filter, or the
  dedicated JSON/All export menu entries.

## Artifacts

For an input `hero.lvks` the exporter writes into the output directory:

| File        | Content                                              |
| ----------- | ---------------------------------------------------- |
| `hero.png`  | Packed RGBA sprite sheet (shelf packing, power-of-two dimensions, 16384 px cap per side). |
| `hero.json` | Atlas descriptor: frame rects inside `hero.png` + animation sequences. |

The pair is written **atomically as a pair**: the PNG goes to a `.tmp`
sibling first and is only renamed into place after the JSON was
successfully written (via `QSaveFile`), so a failed export never leaves
a fresh `.png` next to a stale `.json` or vice versa.

Only frames **used by at least one animation** are packed (same policy
as the Cocos2d exporter's `isFrameUnused()` skip).

## Descriptor schema

Top-level keys: `frames`, `animations`, `meta`.

```json
{
  "frames": {
    "walk_l.png": {
      "filename": "walk_l.png",
      "frame":            { "x": 86, "y": 0, "w": 29, "h": 46 },
      "rotated": false,
      "trimmed": false,
      "spriteSourceSize": { "x": 86, "y": 0, "w": 29, "h": 46 },
      "sourceSize":       { "w": 29, "h": 46 },
      "duration": 180
    }
  },
  "animations": {
    "walk": [ "walk_l.png", "walk_r.png" ]
  },
  "meta": {
    "app": "LvkSpriteEditor",
    "version": "2.0",
    "image": "hero.png",
    "format": "RGBA8888",
    "size": { "w": 128, "h": 128 },
    "scale": "1"
  }
}
```

### `frames`

A hash keyed by frame name + `.png` (TexturePacker convention). Notes:

- **Key derivation:** the frame's `name` from the `.lvks`, falling back
  to `id_<N>` for unnamed frames. Duplicate names get a `_2`, `_3`, ...
  suffix *before* the `.png` so keys stay unique (Team H2.3).
- `frame` is the rect inside the packed sheet. The exporter neither
  rotates nor trims, so `rotated`/`trimmed` are always `false` and
  `spriteSourceSize`/`sourceSize` mirror `frame`.
- `duration` (milliseconds) is an LVK extension: the delay of the
  *first* aframe that references this frame anywhere in the sprite
  (TexturePacker's schema has no per-instance delay; per-aframe delays
  are only fully represented in the Cocos2d `.lkot` output).

### `animations` (LVK extension)

Maps each animation name to its frame keys **in playback order**
(the in-memory aframe order, which matches the id order the `.lvks`
canonical form serializes). A frame key may repeat (held frames).
Consumers that only understand stock TexturePacker JSON can ignore this
key entirely.

### `meta`

Standard TexturePacker block: `image` names the sibling sheet,
`size` its pixel dimensions, `format` is always `RGBA8888`, `scale`
always `"1"`.

## Limits and failure modes

- Frame dimensions are capped at 8192 px per side at `.lvks` load time;
  the packed sheet is capped at **16384 px per side**. If the shelf
  packing would need more than that (accounting for end-of-shelf
  waste), the export **fails** rather than clipping frames
  (`tests/security/tst_atlas_size_bomb.cpp`).
- A used frame whose source image cannot be read fails the whole export
  (`SpriteState::ErrCantExportFrame`); no partial artifacts are left.
- Output paths go through the same `isSafeExportPath()` containment
  check as the Cocos2d exporter
  (`tests/format/tst_path_traversal.cpp`).

Schema coverage lives in `tests/format/tst_json_export.cpp`.
