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
`line.trimmed()` in the `load()` read loop. Empty lines and lines
starting with `#` are skipped, except inside `custom_header(...)` where
they are preserved (subject to the whitespace caveat documented in the
`custom_header(...)` section below).

A file that ends before its version header, or inside an unterminated
block (`images(` / `frames(` / `animations(` / `aframes(` /
`custom_header(`), is rejected with `ErrInvalidFormat` -- truncated
files must not "successfully" load partial data
(`tests/format/tst_load_truncated.cpp`).

## Version header

Source: the `HEADER_VER_*` macros near the top of `src/spritestate.cpp`.

| Macro             | String                  | Introduced by                |
| ----------------- | ----------------------- | ---------------------------- |
| `HEADER_VER_01`   | `LvkSprite version 0.1` | initial release              |
| `HEADER_VER_02`   | `LvkSprite version 0.2` | added image `scale` column; aframes gain explicit `ox, oy` |
| `HEADER_VER_03`   | `LvkSprite version 0.3` | added animation `flags` column and the `custom_header()` block |
| `HEADER_VER_04`   | `LvkSprite version 0.4` | added `sticky` flag on aframes |
| `HEADER_LATEST`   | `= HEADER_VER_04`       | upper bound on what `save()` may write; the actual written header is `max(loadedVersion, minimumVersion())` |

The parser accepts any of the four header strings as valid (the
`StCheckVersion` state in `SpriteState::load()`). Anything else
terminates load with `ErrInvalidFormat`.

## Sections

### `images(...)`

Source: `InputImage::fromString` / `toString` in `src/inputimage.cpp`.

Each record is one comma-separated line:

```
imageId,filename,scale
```

- `imageId` -- integer `Id`. Must be unique within the file.
- `filename` -- path to the source image, relative to the `.lvks` file's
  directory. **Validated on load** (see "Load-time validation" below):
  absolute paths, `..` traversal, UNC `\\` prefixes, leading `~`,
  Windows drive prefixes, embedded NUL, and non-whitelisted image
  extensions are all rejected (record skipped).
- `scale` -- optional. Floating-point; defaults to `1.0`. Must lie in
  `[0.001, 16.0]`; out-of-range, non-numeric, or non-finite values
  reject the record.

#### Field-count history

| Field count | Versions      | Behavior                                  |
| ----------- | ------------- | ----------------------------------------- |
| 2           | v0.1+         | `scale = 1.0`, image loaded as-is.        |
| 3           | v0.2+         | Explicit `scale`; image rescaled on load. |

Any other count is rejected (warning, then `ErrInvalidFormat`).

### `frames(...)`

Source: `LvkFrame::fromString` / `toString` in `src/lvkframe.cpp`.

```
frameId,name,imageId,ox,oy,w,h
```

- `frameId` -- integer `Id`, unique within the file.
- `name` -- display name (may not contain `,`).
- `imageId` -- references an `images()` record.
- `ox, oy, w, h` -- bounding rect (integers, pixels) into the source
  image. `w` and `h` must lie in `(0, 8192]` (`LvkFrame::kMaxDim`);
  records outside the bound are skipped on load.

#### Field-count history

Only one field layout has ever been valid: **7 fields**, identical
across v0.1-v0.4. Any other count is rejected.

### `animations(...)` + `aframes(...)`

Animations are written as a header line followed by an indented
`aframes(...)` block.

#### Animation record

Source: `LvkAnimation::fromString` / `toString` in `src/lvkanimation.cpp`.

```
animationId,name,flags
```

- `animationId` -- integer `Id`, unique within the file.
- `name` -- display name.
- `flags` -- unsigned 32-bit bitfield (decimal on disk; the full range
  including values >= `0x80000000` round-trips). Currently used flags
  are documented in `src/lvkanimation.h`.

##### Field-count history

| Field count | Versions     | Behavior                                  |
| ----------- | ------------ | ----------------------------------------- |
| 2           | v0.1 .. v0.2 | `flags = 0` (the field did not exist).    |
| 3           | v0.3+        | Explicit `flags`.                         |

#### Aframe record (animation frame)

Source: `LvkAframe::fromString` / `toString` in `src/lvkaframe.cpp`.

```
aframeId,frameId,delay,ox,oy,sticky
```

- `aframeId` -- integer `Id`. **Not** an index; ids are unique keys.
  (A long-standing bug in `SpriteState::addAframe` treated this as a
  `QList` index and corrupted the heap. Fixed in the Agent 7 pass; see
  `UPGRADE_NOTES.md` #4.) In the **canonical form** written by
  `save()`, aframes are serialized sorted by id, so on-disk id order IS
  the playback order; the loader preserves file order in memory (see
  "Canonical form" below).
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

Source: the `StTokenHeader` state in `SpriteState::load()`. Introduced
in v0.3.

Free-form lines, one per input line. Whatever is between the opening
`custom_header(` and the closing `)` is captured into
`SpriteState::_customHeader` (with `\n` appended per line). At export
time the captured text is emitted into the generated `.h` header
**after** the `#define ANIM_...` macro block (between it and the
closing `#endif`), fenced by `// starting custom header data` /
`// end custom header data` markers -- so games can stick `#include`
lines, copyright notices, or `#define` macros there.

**Whitespace caveat.** The loader reads each line via
`stream.readLine().trimmed()`, which strips **both** leading and
trailing whitespace before the line reaches the `custom_header(...)`
branch. As a result, neither indentation nor trailing whitespace inside
the block survives a load/save cycle (interior whitespace does). For
example, given an input of:

```
custom_header(
    #include <foo.h>
        // indented comment
#define BAR 1
)
```

what is captured (and re-emitted) is:

```
custom_header(
#include <foo.h>
// indented comment
#define BAR 1
)
```

This is benign for the intended use case (C preprocessor directives,
which do not depend on leading whitespace).

The canonical form is a **fixed point**: `save()` strips trailing
whitespace from the captured header and writes exactly one terminating
newline, so repeated save/load cycles are byte-stable. (An earlier bug
grew one trailing newline per cycle -- `UPGRADE_NOTES.md` #5, fixed by
Agent 8 and pinned by
`tst_lvks_roundtrip::roundtripPreservesCustomHeader`.)

### Transitions

The `.lvks` format historically reserved a `transitions(...)` block as a
sibling of `animations(...)` to describe state transitions between named
animations. The feature is **not currently implemented**: there is no
public API to author transitions, the UI button that used to open the
transitions dialog is disabled (Team D4), and `SpriteState::save()` does
not emit a `transitions(...)` block.

The loader is tolerant of the block: if a hand-edited file (or a
forward-compatible producer) includes one, the parser recognises the
opening `transitions(` token and consumes lines until the matching
`)`, then issues a `qWarning()` on stderr:

```
SpriteState::load(): transitions() block found in "<path>" at line N
but transitions are not yet implemented; block contents will be
dropped on save.
```

The block's payload is **dropped** -- the in-memory model has no slot to
hold it, and saving the file does not write it back. To make the silent
drop visible in the saved file, `save()` emits a comment near the end
of the file:

```
# transitions intentionally dropped -- feature not implemented
```

If/when transitions are implemented, both the warning and the comment
should be removed in lockstep with the load/save plumbing that
preserves the block.

## Load-time validation

The loader is deliberately stricter than the historical Qt4 parser.
Rejected **records** are skipped with a `qWarning()` and counted; the
count surfaces to GUI users as a partial-load dialog and to CLI users
as exit code `2` ("export ran, artifacts are missing data"). Rejected
**files** (bad/missing version header, truncated blocks) fail the whole
load with `ErrInvalidFormat`.

Per-record gates:

- image `filename`: path safety (`lvk::isSafeImagePath` -- no absolute
  paths, `..` segments, UNC `\\`, leading `~`, drive prefixes, NUL) and
  the image-format extension/content whitelist
  (`lvk::validateImageFile`: png/jpg/jpeg/bmp/gif/webp/xpm/xbm/tif/tiff;
  note SVG is deliberately excluded). A whitelisted file that is
  *absent on disk* is admitted with a null pixmap (broken-asset-link
  editing state); a *present* file must also pass a decoder sniff.
- image `scale`: bounded to `[0.001, 16.0]`, finite, numeric.
- frame `w`/`h`: `(0, 8192]` (`LvkFrame::kMaxDim`).
- record arity: see the field-count tables above.

## Canonical form and round-trip guarantee

`save()` normalizes on write:

- **Aframes are serialized sorted by id** within each animation (the
  in-memory list order is what playback and the JSON exporter follow;
  GUI reorder operations keep ids position-stable so the sort is
  a no-op for GUI-authored states). The `.lkot` exporter applies the
  same sort -- see `cocos2d-export.md`.
- `custom_header` is trailing-whitespace-stripped with exactly one
  terminating newline (fixed point; see above).
- The version header written is `max(loadedVersion, minimumVersion())`.

Round-trip coverage (`tests/format/tst_lvks_roundtrip.cpp`,
`tests/format/tst_aframe_order.cpp`):

- `examples/mario.lvks` and `examples/ryu.lvks` are loaded, saved, and
  reloaded; the result is asserted to be **structurally equivalent** to
  the input -- same header version, same set of images / frames /
  animations, same per-record key fields and ordering. Byte-level
  equivalence is **not** guaranteed for arbitrary hand-authored input
  (the loader is lenient -- e.g. it accepts comma counts of 3/5/6 for
  `LvkAframe::fromString` -- and `save()` rewrites in the canonical
  form); a file already in canonical form round-trips byte-stably.
- `tests/format/tst_lvks_versions.cpp` enumerates every accepted version
  header.

If you bump the format, add a new `HEADER_VER_0x` macro, extend the
`fromString()` chain for the affected data class, and add a fixture for
the new version. Do not delete or reorder the existing version branches
-- doing so silently breaks old projects.

## Source-of-truth cross-references

Function-level references (line numbers rot; use your editor's symbol
search):

- Version macros: `HEADER_VER_*` in `src/spritestate.cpp`.
- Top-level layout: `SpriteState::save()` (write) and
  `SpriteState::load()` (read) in `src/spritestate.cpp`.
- Per-record parsers:
  - `InputImage::fromString` -- `src/inputimage.cpp`.
  - `LvkFrame::fromString` -- `src/lvkframe.cpp`.
  - `LvkAnimation::fromString` -- `src/lvkanimation.cpp`.
  - `LvkAframe::fromString` -- `src/lvkaframe.cpp`.
- Validation: `lvk::isSafeImagePath` (`src/inputimage.cpp`),
  `lvk::validateImageFile` (`src/image_validation.cpp`).

## See also

- [`cocos2d-export.md`](cocos2d-export.md) -- schema for the runtime
  Cocos2d export (`.lkob` / `.lkot` / `AnimNameDef_<base>.h`)
  produced by `SpriteState::exportSprite()`. The `.lvks` file is the
  authoring format; the Cocos2d trio is the artifact the game loads.
