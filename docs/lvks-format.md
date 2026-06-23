LvkSprite (`.lvks`) Text Format
================================

This document is the formal specification of the `.lvks` sprite-project
file format produced and consumed by `SpriteState::save()` and
`SpriteState::load()` (`src/spritestate.cpp`). The format is text, line
oriented, and self-versioning -- every file begins with a single header
line that names the version, and each data class accepts multiple field
counts in its `fromString()` for backward compatibility.

This spec covers **v0.1 through v0.4** (latest). Read compatibility for
all four is preserved. `save()` writes the lowest version sufficient to
represent the in-memory data (the max of the loaded version and the
minimum-required version). v0.4 only when sticky is used; v0.3 only when
animation flags or a `custom_header` is set; v0.2 only when image scale
!= 1.0; otherwise v0.1.

## File layout

```
### LvkSprite #########################################
LvkSprite version 0.4

# Images
# format: imageId,filename,scale
images(
    <image-record>
    ...
)

# Frames
# format: frameId,name,imageId,ox,oy,w,h
frames(
    <frame-record>
    ...
)

# Animations
# format: animationId,name,flags
# Animation frames
# format: aframeId,frameId,delay,ox,oy,sticky
animations(
    <animation-record>
    aframes(
        <aframe-record>
        ...
    )
    ...
)

# Custom data appended to the header
custom_header(
    <free-form text>
)

### End LvkSprite #####################################
```

Whitespace is significant only to the extent that records are
**one-per-line**. Leading tabs/spaces inside a section are stripped by
`line.trimmed()` (`src/spritestate.cpp:281`). Empty lines and lines
starting with `#` are skipped, except inside `custom_header(...)` where
both are preserved verbatim.

## Version header

Source: `src/spritestate.cpp:71-75`.

| Macro             | String                  | Introduced by                |
| ----------------- | ----------------------- | ---------------------------- |
| `HEADER_VER_01`   | `LvkSprite version 0.1` | initial release              |
| `HEADER_VER_02`   | `LvkSprite version 0.2` | (no on-disk field change)    |
| `HEADER_VER_03`   | `LvkSprite version 0.3` | added `custom_header()` block |
| `HEADER_VER_04`   | `LvkSprite version 0.4` | added `sticky` flag on aframes |
| `HEADER_LATEST`   | `= HEADER_VER_04`       | upper bound on what `save()` may write; the actual written header is `max(loadedVersion, minimumVersion())` |

The parser accepts any of the four header strings as valid
(`spritestate.cpp:294-306`). Anything else terminates load with
`ErrInvalidFormat`.

## Sections

### `images(...)`

Source: `src/inputimage.cpp:25-49`.

Each record is one comma-separated line:

```
imageId,filename,scale
```

- `imageId` -- integer `Id`. Must be unique within the file.
- `filename` -- path to the source image, relative to the `.lvks` file's
  directory.
- `scale` -- optional. Floating-point; defaults to `1.0`.

#### Field-count history

| Field count | Versions      | Behavior                                  |
| ----------- | ------------- | ----------------------------------------- |
| 2           | v0.1+         | `scale = 1.0`, image loaded as-is.        |
| 3           | v0.2+         | Explicit `scale`; image rescaled on load. |

Any other count is rejected (warning, then `ErrInvalidFormat`).

### `frames(...)`

Source: `src/lvkframe.cpp:31-48`.

```
frameId,name,imageId,ox,oy,w,h
```

- `frameId` -- integer `Id`, unique within the file.
- `name` -- display name (may not contain `,`).
- `imageId` -- references an `images()` record.
- `ox, oy, w, h` -- bounding rect (integers, pixels) into the source image.

#### Field-count history

Only one field layout has ever been valid: **7 fields**, identical
across v0.1-v0.4. Any other count is rejected.

### `animations(...)` + `aframes(...)`

Animations are written as a header line followed by an indented
`aframes(...)` block.

#### Animation record

Source: `src/lvkanimation.cpp:25-43`.

```
animationId,name,flags
```

- `animationId` -- integer `Id`, unique within the file.
- `name` -- display name.
- `flags` -- bitfield. Currently used flags are documented in
  `src/lvkanimation.h`.

##### Field-count history

| Field count | Versions  | Behavior                                  |
| ----------- | --------- | ----------------------------------------- |
| 2           | v0.1      | `flags = 0` (the field did not exist).    |
| 3           | v0.2+     | Explicit `flags`.                         |

#### Aframe record (animation frame)

Source: `src/lvkaframe.cpp:30-62`.

```
aframeId,frameId,delay,ox,oy,sticky
```

- `aframeId` -- integer `Id`. **Not** an index; the order is the
  authoritative play order. (Note: a long-standing bug in
  `SpriteState::addAframe` treated this as a `QList` index and corrupted
  the heap. Fixed in the Agent 7 pass; see `UPGRADE_NOTES.md` #4.)
- `frameId` -- references a `frames()` record.
- `delay` -- duration in milliseconds the frame stays on-screen.
- `ox, oy` -- per-aframe pixel offset, added to the frame's own rect.
- `sticky` -- bool (0/1). When set, the renderer holds this aframe's
  pose across animation transitions until a non-sticky aframe overrides.

##### Field-count history

| Field count | Versions     | Behavior                                            |
| ----------- | ------------ | --------------------------------------------------- |
| 3           | v0.1         | `ox = oy = 0`, `sticky = false`. Pure timing entry. |
| 5           | v0.2 .. v0.3 | Adds explicit `ox, oy`. `sticky = false`.           |
| 6           | v0.4+        | Adds `sticky` flag.                                 |

Any other count is rejected.

### `custom_header(...)`

Source: `src/spritestate.cpp:405-411`. Introduced in v0.3.

Free-form lines, one per input line. Whatever is between the opening
`custom_header(` and the closing `)` is captured **verbatim** into
`SpriteState::_customHeader` (with `\n` appended per line). At export
time the captured text is emitted unchanged into the generated `.h`
header above the macro definitions, so games can stick `#include` lines,
copyright notices, or `#define` macros there.

Known bug: each save/load cycle currently appends one extra trailing
newline (`UPGRADE_NOTES.md` #5). Tracked for Agent 8's
`src/spritestate.cpp` follow-up.

## Round-trip guarantee

Per Phase-1 Agent 2's work:

- `examples/mario.lvks` and `examples/ryu.lvks` are loaded, saved, and
  the saved text is compared byte-for-byte to the input in
  `tests/format/tst_lvks_roundtrip.cpp`.
- `tests/format/tst_lvks_versions.cpp` enumerates every accepted version
  header.

If you bump the format, add a new `HEADER_VER_0x` macro, extend the
`fromString()` chain for the affected data class, and add a fixture for
the new version. Do not delete or reorder the existing version branches
-- doing so silently breaks old projects.

## Source-of-truth cross-references

- Version macros: `src/spritestate.cpp:71-75`.
- Top-level layout: `src/spritestate.cpp:182-225` (write),
  `src/spritestate.cpp:280-422` (read).
- Per-record parsers:
  - `InputImage::fromString` -- `src/inputimage.cpp:25-49`.
  - `LvkFrame::fromString` -- `src/lvkframe.cpp:31-48`.
  - `LvkAnimation::fromString` -- `src/lvkanimation.cpp:25-43`.
  - `LvkAframe::fromString` -- `src/lvkaframe.cpp:30-62`.
