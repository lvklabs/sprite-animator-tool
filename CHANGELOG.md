Changelog
=========

All notable changes to the LVK Sprite Animator Tool are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-05-16

The **2026 modernization**, delivered by a 10-agent upgrade pass on the
`upgrade-sprite-animator-O2rKz` branch. Read compatibility with
every `.lvks` v0.1-v0.4 file produced by the legacy 1.x line is
preserved. The Cocos2d `.lkob` / `.lkot` / `.h` export pipeline is
unchanged, and golden-file round-trip tests pin its byte-for-byte
output.

### Added

- **CMake 3.21+ build** with `CMakePresets.json` (`default`, `debug`,
  `asan` presets). Replaces the legacy qmake `LvkSpriteEditor.pro`.
- **Unit test suite** (Qt Test) covering data-class CRUD, undo/redo
  invariants, `fromString`/`toString` round-trips, and `SpriteState`
  load/save. 9 ctest targets across `tests/unit/`, `tests/format/`, and
  `tests/security/`.
- **CI/CD matrix** on GitHub Actions (Linux x macOS x Windows x Qt 6.8)
  with `jurplel/install-qt-action` + `aqtinstall` cache.
- **`.clang-format`**, **`.clang-tidy`**, **`.editorconfig`** for
  baseline code-style enforcement.
- **CPack packaging** stubs for DEB / TGZ (Linux), DragNDrop / DMG
  (macOS), NSIS / ZIP (Windows).
- **Modern CLI** via `QCommandLineParser` (`src/main.cpp`). New
  short and long options: `-e/--export`, `-o/--output-dir`,
  `-p/--postprocessing-script`, `-f/--format`, `-h/--help`,
  `-v/--version`.
- **`--format=cocos2d|json|all`** flag, wiring through a new generic
  JSON sprite-atlas exporter (TexturePacker-style JSON + PNG sprite
  sheet) plus the existing Cocos2d trio.
- **Documentation**: new `docs/lvks-format.md` (formal `.lvks` format
  spec with version-history per record), `CONTRIBUTING.md`, this
  `CHANGELOG.md`, and a fully rewritten `README.md`.
- **`UPGRADE_NOTES.md`** -- a per-agent log of issues encountered and
  resolved (or deferred) during the upgrade.

### Changed

- **Qt 4.5.2 -> Qt 6.4+.** All `#include <QtGui/QFoo>` shifted to
  `#include <QFoo>`, `Qt::SkipEmptyParts` replaces
  `QString::SkipEmptyParts`, `QApplication::AA_EnableHighDpiScaling`
  removed (no-op in Qt 6).
- **`SIGNAL()`/`SLOT()` -> pointer-to-member-function.** ~100
  connections in `mainwindow.cpp` converted to type-checked PMF syntax;
  `QOverload<int>::of(&QComboBox::activated)` for ambiguous overloads.
- **`QRegExp` -> `QRegularExpression`** at all call sites.
- **`QMouseEvent::x()/y()` -> `event->position().x()/y()`** in the
  three widget event handlers (`UPGRADE_NOTES.md` #2).
- **`QHash` -> `QMap`** for serialised collections so iteration order
  is deterministic in Qt 6 (originally landed in pre-fork commit
  `a4971f2`, preserved through the upgrade).
- **Splash About dialog gated by `QSettings("ui/showAboutOnStartup")`.**
  Defaults to `true` to preserve legacy behavior, but no longer blocks
  headless CLI invocations because the CLI is now parsed before
  `MainWindow` is instantiated (`UPGRADE_NOTES.md` #9).
- **`MainWindow` refactor.** The 2,511-line god class is split into a
  thin `MainWindow` + per-tab controllers (`ImageTabController`,
  `FrameTabController`, `AnimationTabController`,
  `TransitionTabController`, `ExportController`) under
  `src/controllers/`.

### Fixed

- **CRITICAL: Shell injection via `--postprocessing-script`.**
  `SpriteState::writeImageWithPostprocessing` used
  `QProcess::start(QString)` which interprets the argument as a
  shell-style command line. A `--postprocessing-script` value
  containing shell metacharacters (`;`, `|`, `&&`, `$()`) or paths
  containing whitespace could execute arbitrary commands. Now uses
  `QProcess::start(program, args)` with `QProcess::splitCommand()` on
  the trusted-shape portion only, plus `QTemporaryFile`-backed scratch
  images and script-path validation. CVE-class fix landed by Agent 5
  with a regression test in `tests/security/`.
- **`SpriteState::addAframe` heap corruption** on non-contiguous aframe
  ids. Replaced the OOB `QList::insert(aframe.id, aframe)` with
  `append(aframe)` -- matches the parser/save iteration order
  (`UPGRADE_NOTES.md` #4).
- **`SpriteState::aframes(Id)` returned a dangling reference** under
  Qt 6 (`QMap::operator[] const` now returns by value). Rewritten with
  a const iterator + static empty-list sentinel
  (`UPGRADE_NOTES.md` #1).
- **`SpriteState::const_image/frame/animation/aframe(Id)`** were
  silently inserting default-constructed entries on missing keys via
  `operator[] const`. Switched to `constFind()` with a static null
  sentinel.
- **`LvkAction` leaked on every recent-file menu rebuild** -- the
  `new LvkAction(filename)` calls had no parent and `QMenu::addAction`
  does not take ownership. Now parented to `MainWindow`.
- **`QComboBox::activated(QString)` removed in Qt 6** -- the
  `previewScrSizeCombo` connection compiled but silently no-op'd at
  runtime. Rewired with `QOverload<int>::of(&QComboBox::activated)` +
  `itemText(index)` lookup (`UPGRADE_NOTES.md` #3).
- **`--version` and `--help` no longer hang.** The legacy entry point
  constructed `MainWindow` -- which popped a modal About dialog from
  its constructor -- before parsing CLI flags. Restructured to parse
  CLI first; `MainWindow` is only instantiated on the interactive path
  (`UPGRADE_NOTES.md` #9).

### Security

See the "Fixed" section above for the shell-injection fix
(`writeImageWithPostprocessing`). Audit logs are in the prologue of
`src/spritestate.cpp`. Two further hardening items (output-path
traversal in `exportSprite`, postprocessor output-size cap) are tracked
as `[FIXME(agent-5)]` markers for a future pass.

### Removed

- `src/LvkSpriteEditor.pro` -- the legacy qmake project. Replaced by
  the root `CMakeLists.txt`.
- The hand-rolled `parseCmdLine` / `showHelp` / `showVersion`
  functions in `src/main.cpp` (with their famous
  `// TODO use getopt !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!`
  comment). Replaced by `QCommandLineParser`.
- The `_pCache[1000][7]` raw-pointer `QPixmap*` cache in
  `LvkInputImageWidget`. Replaced by `QCache<QPair<Id,int>, QPixmap>`
  with LRU eviction; eliminates the hardcoded bounds check and ~25 LOC
  of hand-rolled lifetime management.

### Migration notes

- **Build:** `cmake --preset=default && cmake --build build`. The
  qmake instructions in the legacy README are no longer valid.
- **CLI:** the legacy `--export sprite.lvks --output-dir DIR` style
  still works (positional + named option). Add `--format=json` or
  `--format=all` to opt into the new exporter.
- **Format:** existing v0.1-v0.4 `.lvks` files load unchanged. The
  saver always emits v0.4.

### Contributors

Ten parallel agent passes (Phases 0-3) on
`upgrade-sprite-animator-O2rKz`. See `UPGRADE_NOTES.md` for the
per-agent issue log.

## [1.2.1] - 2014

Last release of the legacy Qt 4 line. See the git history before
`15981a6 Agent 1/10: CMake build system + mechanical Qt6 port` for the
1.x changes; no formal changelog was kept before the 2026 upgrade.
