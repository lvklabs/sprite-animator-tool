Changelog
=========

All notable changes to the LVK Sprite Animator Tool are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.1] - 2026-06-23

A 6-phase remediation pass triggered by a multi-agent premortem on the
2.0.0 work. The premortem surfaced 15 issues across compatibility,
build/CI fragility, refactor blast radius, security, and UX regression;
this release closes all of them.

### Added

- **Format-version tracking.** New `LvkVersion` enum on `SpriteState`
  with `loadedVersion()` and `minimumVersion()`. The save header is
  now `max(loaded, minimum)` so legacy `.lvks` files round-trip
  without being silently rewritten to v0.4.
- **i18n infrastructure.** `QTranslator` installed at startup;
  skeleton `.ts` files for `en` / `es` / `fr` under `translations/`;
  CMake wired through `qt_add_lrelease` + `qt_add_resources` (gated
  on `Qt6LinguistTools_FOUND` so builds without LinguistTools still
  succeed).
- **Window geometry persistence.** `MainWindow` saves/restores
  geometry + state via `QSettings` (`ui/mainwindow/geometry`,
  `ui/mainwindow/state`).
- **Accessibility.** `accessibleName` / `accessibleDescription` on
  the five table widgets, frame canvas, previews, and primary toolbar
  buttons. Keyboard shortcuts via `QKeySequence::{Open,Save,SaveAs,Undo,Redo,New,Quit}`.
- **10 new test executables** (21 total, up from 11):
  - `tst_version_preservation` — v0.1 round-trip + bump-on-edit
  - `tst_aframe_order` — legacy id-order on load
  - `tst_unicode_names` — kanji macro round-trip + comma rejection
  - `tst_image_path_injection` — UNC / absolute / `..` / NUL rejection
  - `tst_atlas_size_bomb` — overflow guard on JSON atlas exporter
  - `tst_path_traversal_hard` — NUL byte / alt-separator / prefix confusion
  - `tst_script_toctou` — canonical-path symlink defeat
  - `tst_image_tab_controller`, `tst_export_controller`,
    `tst_mainwindow_construction` — refactor-target coverage
- **CI hardening**: per-OS CMake presets; binary smoke step
  (`--help` + headless export) on Linux/macOS; `cpack -G DEB` +
  `dpkg -i` install + invoke on every Linux push; tag-triggered
  `release.yml` workflow.  Effective CI matrix: Linux (apt Qt 6.4.2)
  + macOS (Qt 6.5.3, 6.8.0).  Windows builds via Ninja currently
  fail with a duplicate-AUTOUIC error and are disabled in CI;
  tracked as a follow-up CMake refactor.

### Fixed

- **CRITICAL: CLI break.** `--export` hard-required `--output-dir` in
  2.0.0; restored to the legacy behavior (defaults to input file's
  directory). Success message moved stderr -> stdout in 2.0.0;
  restored to stderr. Exit code changed from -1 to 1/2; restored to
  -1.
- **CRITICAL: `.lkot` export header lied.** Wrote
  `LvkSprite version 0.1` while emitting v0.4 field counts (6-field
  aframes with sticky). Downstream Cocos2d parsers rejected or
  truncated. Header now matches schema.
- **CRITICAL: undo stack wiped on every file open.**
  `MainWindow::cellChangedSignals(bool)` was an empty no-op after the
  controller split; every cellChanged fired through the update slots
  and polluted `StateCircularBuffer`. Replaced with per-controller
  `QSignalBlocker` (RAII).
- **CRITICAL: dark-mode frame-drawing cursor invisible.** Hard-coded
  `Qt::black` / `Qt::gray` / `Qt::blue` pens in `LvkFrameDefWidget`
  replaced with `QPalette::{WindowText,Mid,Highlight}`. Burned-in
  light-checker background dropped in favor of palette-driven fill.
- **Aframe playback order** changed silently in 2.0.0 (Bug #4 fix
  switched `insert(id,…)` to `append(…)`). Now load-time
  `std::sort` by id; user `append` semantics preserved.
- **Recent-files menu leaked `LvkAction` instances** on every
  `setCurrentFile`. Reparented from `MainWindow` to
  `ui->actionOpenRecent` so `QMenu::clear()` actually frees them.
- **ExportController forgot the export target** when re-opening the
  same file; `setCurrentFile` now compares against the previous path.
- **Slot-connection order on `mouseRectChangeFinished`** flipped vs.
  master; moved the `showMouseRect` connect to after controller
  `wireSignals()` so `blendFrameRect` repaint fires first.
- **`save()` returns `false` on comma/NUL in names** (animation, frame,
  aframe, image). Previously the rejected record was silently dropped
  mid-write, leaving a partial `.lvks` on disk that the loader would
  then complain about.
- **Recent-files menu placeholder reinstated.** `QMenu::clear()`
  evicted the `actionNoRecentFiles` "(no recent files)" placeholder,
  leaving an empty submenu with no hint that the feature exists.
  Re-added after every clear.

### Security

- **Image-path injection** in `InputImage::fromString`. Rejects
  absolute paths, UNC (`\\…`), `..`, and embedded NUL bytes. Also
  rejects leading `~` (shell-expansion sink) and Windows
  drive-absolute paths (`C:\…` or `C:/…`) even when the editor is
  running on Linux/macOS, so a malicious `.lvks` carrying a
  drive-letter prefix can't reach the host fs. Image format
  whitelisted via `QImageReader::format()`; the whitelist is
  png/jpg/jpeg/bmp/gif/webp/svg/xpm/xbm/tif/tiff (expanded from the
  initial seven by Team B3). `QImageReader::setAllocationLimit(256MB)`
  applied at startup.
- **Frame dimension overflow** in the JSON atlas exporter.
  `LvkFrame::fromString` rejects `w/h` outside `(0, 8192]`;
  exporter widens `totalArea` to `qint64` and clamps atlas
  dimensions to 16384px, bailing cleanly on overflow.
- **Path-traversal "fix" was incomplete.** The original check used
  `QFileInfo::baseName()` (segment before first dot), which missed
  NUL bytes, Windows alt-separator on Linux, and prefix confusion
  (`/tmp/safe` vs `/tmp/safe-evil`). Replaced with full-filename
  validation + `canonicalPath` + trailing-separator `startsWith`.
- **TOCTOU in postprocessing-script resolver.** Switched from
  `absoluteFilePath()` to `canonicalFilePath()`; predictable
  `*.ppi` tempfile path replaced with `QTemporaryFile` in
  `QStandardPaths::TempLocation`.

### Changed

- **Qt 6 file-dialog filter strings** corrected from the malformed
  `"*.lvks;; *.*"` to the canonical `tr("Lvks files (*.lvks);;All files (*)")`
  form across 5 call sites.
- **`getMacroName`** is Unicode-aware: non-ASCII letters collapse to
  `_`; consecutive underscores deduplicate; leading-digit prefixed;
  call-site disambiguation appends `_2`, `_3` on collision. Empty
  result falls back to `UNNAMED`.
- **Column widths** in `MainWindow::initTables` computed via
  `fontMetrics().horizontalAdvance()` rather than fixed pixel
  constants (HiDPI fix). Hard-coded `<pointsize>8</pointsize>`
  overrides removed from `mainwindow.ui`.
- **Source tree reformatted** with `clang-format-18` per
  `.clang-format` (LLVM base, 4-space indent, 100-col limit).
  No semantic changes; +1688/-2016 lines across 45 files.

### CI

- `CMakePresets.json` now has per-OS presets (`linux`, `macos`,
  `windows`). Windows uses `Visual Studio 17 2022` instead of the
  previously-hardcoded `Unix Makefiles` (which silently failed on
  `windows-latest`).
- `build.yml` matrix per-OS preset; artifact upload path globs
  `build/**/LvkSpriteEditor*` to catch MSVC's `build/Release/`;
  `if-no-files-found: error`.
- `clang-format` dropped `continue-on-error: true`, pinned
  `clang-format-18`, recurses `find src -name '*.cpp' -o -name '*.h'`.
- New `clang-tidy` job with `compile_commands.json`.
- `CPACK_DEBIAN_PACKAGE_DEPENDS` set so `dpkg -i` resolves.
- `release.yml` (new) on tag push: cpack + `softprops/action-gh-release`.

### Migration notes

- **CLI behaviour** restored to legacy semantics. Existing build
  scripts that used `LvkSpriteEditor --export foo.lvks` without
  `--output-dir` now work again.
- **Format compatibility.** A `.lvks` opened from a v0.1 file is
  saved as v0.1 by default. The header bumps to v0.2/0.3/0.4 only
  when the corresponding feature (image scale, animation flags,
  aframe sticky) is actually used. CLI auto-bumps with a stderr
  warning; GUI bump prompt is planned.
- **Local translations.** If you build with `qt6-tools-dev`
  installed, `lrelease` compiles the `.ts` skeletons and embeds
  them under `:/i18n`. Without it, the binary still works (English
  only).

### Contributors

A 5-agent premortem (compatibility, build/CI fragility, refactor
blast radius, security, UX) followed by 11 commits across 6 phases.
See the git log for per-phase detail.

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
- **CI/CD** on GitHub Actions: Linux (apt Qt 6.4.2) + macOS
  (Qt 6.5.3, 6.8.0) via `jurplel/install-qt-action` +
  `aqtinstall` cache.  Windows builds via Ninja currently fail
  with a duplicate-AUTOUIC error and are disabled in CI; tracked
  as a follow-up CMake refactor.
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
- **Format:** existing v0.1-v0.4 `.lvks` files load unchanged. Since
  2.0.1, the saver preserves the loaded version unless newer-only
  features (sticky, flags, scale) are actually used; on bump, a
  warning is emitted on stderr (CLI) or a confirmation dialog is
  raised (GUI; planned).

### Contributors

Ten parallel agent passes (Phases 0-3) on
`upgrade-sprite-animator-O2rKz`. See `UPGRADE_NOTES.md` for the
per-agent issue log.

## [1.2.1] - 2014

Last release of the legacy Qt 4 line. See the git history before
`15981a6 Agent 1/10: CMake build system + mechanical Qt6 port` for the
1.x changes; no formal changelog was kept before the 2026 upgrade.
