LVK Sprite Animator Tool
========================

A WYSIWYG 2D sprite animation tool for Cocos2d-iphone and Cocos2d-x. This
tool was originally written to animate a complex character named "Julia" in
the game "Julia: A Pick Up Adventure" by LVK Labs. It supports Linux,
Windows, and macOS.

The original LVK Labs description (2010-2014) noted that the project was
"outated" -- this fork is the **2026 modernization**: same `.lvks` /
`.lkob` / `.lkot` / `.h` Cocos2d output, Qt 6 + CMake build, unit tests,
CI matrix, and a new generic JSON sprite-atlas export path.

### Compile

Dependencies: **Qt 6.4+**, **CMake 3.21+**, a C++17 compiler.

```sh
cmake --preset=default
cmake --build build -j$(nproc)
```

The build presets live in `CMakePresets.json` (`default`, `debug`,
`asan`). The executable lands at `build/LvkSpriteEditor`.

To run the test suite (Qt Test):

```sh
ctest --test-dir build --output-on-failure
```

### Usage

#### GUI

```sh
./build/LvkSpriteEditor                       # blank session
./build/LvkSpriteEditor examples/mario.lvks   # open an existing sprite
```

#### Headless export

```sh
./build/LvkSpriteEditor --export examples/mario.lvks \
                        --output-dir /tmp/out
```

This writes the Cocos2d export trio: `mario.lkob` (binary frame pixmaps),
`mario.lkot` (text metadata), and `AnimNameDef_mario.h` (animation-name
macros).

Optional flags:

- `-e`, `--export` -- enable headless mode.
- `-o`, `--output-dir <dir>` -- required with `--export`.
- `-p`, `--postprocessing-script <script>` -- per-frame image filter. The
  script receives `<input-png> <output-png>` and must produce
  `<output-png>`. See `tests/security/tst_postpscript_injection.cpp` for
  the hardened invocation semantics (Agent 5 fix for shell injection).
- `-f`, `--format {cocos2d|json|all}` -- select export format. `cocos2d`
  is the default and currently the only fully wired option in this
  worktree; `json` and `all` will route through the new JSON sprite-atlas
  exporter once Agent 8's branch is merged.
- `-v`, `--version` -- print version and exit.
- `-h`, `--help` -- print usage and exit.

### Architecture

The on-disk format is documented in [`docs/lvks-format.md`](docs/lvks-format.md).
A short tour of the upgraded codebase:

| Path                                  | Role                                          |
| ------------------------------------- | --------------------------------------------- |
| `src/main.cpp`                        | CLI entry (`QCommandLineParser`).             |
| `src/mainwindow.{h,cpp,ui}`           | Top-level `QMainWindow`.                      |
| `src/spritestate.{h,cpp}`             | `.lvks` load/save + export pipeline.          |
| `src/spritestate2.{h,cpp}`            | Undo/redo wrapper around `SpriteState`.       |
| `src/statecircularbuffer.{h,cpp}`     | Bounded undo history.                         |
| `src/lvk{frame,aframe,animation}.cpp` | Data classes + versioned `fromString` chains. |
| `src/inputimage.cpp`                  | Source-image record.                          |
| `src/lvkinputimagewidget.cpp`         | Frame-definition canvas (`QCache` zoom-pixmap cache). |
| `tests/format/`                       | Golden-file round-trip + version tests.       |
| `tests/unit/`                         | Data-class CRUD / undo / round-trip tests.    |
| `tests/security/`                     | Shell-injection regression test.              |

### Modernized 2026

Originally Qt 4.5.2 / qmake. This fork (`claude/upgrade-sprite-animator-O2rKz`)
ships the following changes; see [`CHANGELOG.md`](CHANGELOG.md) for the
per-agent breakdown:

- Ported to **Qt 6.4+** and **CMake 3.21+** with `CMakePresets.json`,
  `AUTOMOC`/`AUTOUIC`/`AUTORCC`.
- Replaced ~100 `SIGNAL()`/`SLOT()` macro connections with type-checked
  pointer-to-member-function syntax.
- 9 unit / format / security test binaries (Qt Test).
- GitHub Actions CI matrix (Linux/macOS/Windows x Qt 6.8), `.clang-format`,
  `.clang-tidy`, `.editorconfig`, CPack packaging stubs (DEB / TGZ / DMG /
  NSIS).
- **Security:** the `--postprocessing-script` shell-out is now
  `QProcess::start(program, args)` instead of the single-string overload,
  with `QTemporaryFile`-backed scratch images and script-path validation
  (Agent 5).
- Memory: raw `new`/`delete` removed in favor of `std::unique_ptr` /
  Qt parent ownership; the `1000x7` raw `QPixmap*` cache became a
  `QCache<QPair<Id,int>, QPixmap>` (Agent 7).
- Latent-bug fixes for `SpriteState::addAframe` (heap corruption on
  non-contiguous aframe ids) and `aframes(Id)` (dangling reference under
  Qt6 `QMap::operator[] const`).
- New `QCommandLineParser`-based CLI (`--version`/`--help` no longer hang
  on a modal About dialog).
- New `--format=cocos2d|json|all` flag (JSON sprite-atlas exporter wired
  through Agent 8's `ExportController`).

The original `.lvks` v0.1-v0.4 read path is preserved unchanged; golden
round-trip tests against `examples/mario.lvks` and `examples/ryu.lvks`
lock the byte-for-byte serialisation.

### Screenshots

![!Tab to define frames](http://www.lvklabs.com/wp-content/uploads/2014/04/lvk-sprite-editor-frames-tab.png)
*Tab to define frames*

![!Tab to define animations](http://www.lvklabs.com/wp-content/uploads/2014/04/lvk-sprite-editor-animations-tab.png)
*Tab to define animations*

![!Tab to test transitions between animations](http://www.lvklabs.com/wp-content/uploads/2014/04/lvk-sprite-editor-transitions-tab.png)
*Tab to test transitions between animations*

### How to use the exported artifacts in a game

1. Create a sprite file (or open an example from the `examples/` folder).
2. Create / edit your animations in the GUI.
3. Export with **File -> Export** (or `--export` on the CLI).
4. Include the generated `.h`, `.lkot`, and `.lkob` files in your game.
5. Link the [LvkSprite-iphone](https://github.com/lvklabs/lvksprite-iphone)
   or [LvkSprite-x](https://github.com/lvklabs/lvksprite-x) runtime
   library and load the files at startup.

### Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for code style, test policy,
branch policy, and how to run `ctest`.

### License

GPL-3.0-or-later. See `COPYING` for the full text, `LICENSE` for the
short notice.
