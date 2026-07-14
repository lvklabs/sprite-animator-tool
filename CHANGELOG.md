Changelog
=========

All notable changes to the LVK Sprite Animator Tool are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Round 7: a nine-dimension validation audit of the upgrade (Qt6 API
completeness, ASan/UBSan, coverage, correctness review, docs drift, CI
health, runtime smoke, i18n, resolution cross-check) followed by fixes
for everything the audit confirmed. Independent verification of the
baseline: zero-warning build under `-DQT_DISABLE_DEPRECATED_BEFORE=0x060900
-Wall -Wextra`, clean ASan/UBSan test-suite run (after the fix below),
zero LeakSanitizer reports.

### Fixed

- **UB: uninitialized `LvkFrameDefWidget::_resizingRect`** read on the
  first paint (10 UBSan invalid-enum diagnostics under the `asan`
  preset; wrong paint/cursor branches until the first mouse release in
  release builds). Also initialized `_mouseClickX/_mouseClickY`.
- **Orphan frames on image removal**: the frame-cascade loop in
  `ImageTabController::removeImage()` iterated forward over the table
  while `removeFrame()` shifted rows up, so consecutive dependent
  frames were only half-removed and dangling `imgId` records were
  written to disk. Now iterates backwards; regression test added.
- **Ghost animations via the editable Id column**: the animations
  table's Id cells were editable, and every downstream lookup then used
  the phantom id — inserting ghost animations into state through
  `QMap::operator[]` and writing through `LvkAnimation::aframe()`'s
  shared static sentinel. Id cells are now non-editable at the item
  level, and all `SpriteState`/`SpriteState2` `update*`/`remove*`
  methods reject unknown ids with a warning no-op instead of
  ghost-inserting (new `LvkAnimation::hasAframe()`).
- **Undo of an aframe removal appended at the end** instead of
  restoring the original playback position (visible reorder in the
  preview and JSON export). The removal now records the list index and
  undo re-inserts at it (new `SpriteState::insertAframe`).
- **GUI aframe reorder was silently lost on save**: `save()`
  serializes aframes sorted by id, but Move Up/Down swapped list
  positions with ids riding along, so the next save/load undid the
  reorder. `swapAframes()` now keeps ids position-stable (content
  swaps, ids stay), making reorders persistent; pinned by tests.
- **JSON atlas overflow guard measured area, not extent**: shelf-packed
  frames could pass the area check yet need more height than the
  16384 px cap, silently clipping frames out of the PNG while the JSON
  recorded out-of-bounds coordinates. The guard now fails on required
  extent (max frame width / shelf-packed height).
- **Export "succeeded" with corrupt output when a frame image was
  missing/unwritable**: `writeImageWithPostprocessing()`'s return value
  was discarded, emitting zero-length `.lkob` records with exit 0. The
  export now fails with the new `ErrCantExportFrame` and removes
  partial artifacts.
- **Truncated/empty `.lvks` loaded "successfully"**: EOF before the
  version header or inside an unterminated block now fails with
  `ErrInvalidFormat` instead of silently loading partial data (a
  headless export of such a file exited 0 with empty artifacts).
- **Headless CLI aborted with SIGABRT on display-less machines**:
  `--export`/`--version`/`--help` now fall back to the `offscreen` Qt
  platform on Linux/BSD when no `DISPLAY`/`WAYLAND_DISPLAY` is set.
- **Relative `--output-dir`/`-p` resolved against the wrong directory**:
  both were validated against the invocation CWD but resolved after the
  chdir to the sprite's directory; now absolutized up front.
- **Validation asymmetries (delayed data loss)**: GUI paths accepted
  values the hardened loader later rejected — image filename cell edits
  (no whitelist/path-safety gates; absolute paths now auto-relativized
  and gated, also on `addImage`), frame W/H cell edits (now bounded to
  the loader's 1..8192 via `LvkFrame::kMaxDim`), and image scale
  (now bounded to [0.001, 16] on load, cell edit, and `scale()`, which
  also guards its double→int conversion).
- **Ctrl+E re-export always ran Cocos2d** even when the user's last
  Export As picked JSON/All, leaving stale atlases; the format is now
  remembered alongside the filename.
- **`isSafeExportPath` rejected any `..` substring**, failing
  legitimate names like `hero..final.lvks`; only `..` path segments are
  traversal now.
- **Animation flags ≥ 0x80000000 were zeroed on load** (`toInt`
  overflow; the hex cell edit had the same bug via `toInt(&ok, 16)`);
  both parse with `toUInt` now and the full 32-bit range round-trips.
- **Per-frame export progress printed to stdout**, violating the
  all-diagnostics-to-stderr CLI contract; moved to stderr (export
  stdout is now empty).
- **`setShortcut(StandardKey)` erased Save As / Quit shortcuts** on
  platform themes whose standard-key list is empty (generic Unix
  theme); standard actions now bind the full `keyBindings()` list and
  keep the `.ui` literal as fallback — Redo gains `Ctrl+Shift+Z` /
  `Alt+Shift+Backspace` on Linux.
- **`--help` claimed `--output-dir` was required** with `--export`; it
  defaults to the sprite's directory.
- **`writePostprocImage()` read the postprocessing script's output
  unbounded**; now capped at 256 MB (closing the last open
  `FIXME(agent-5)` hardening item) with short-write detection.

### Added

- **Four new test executables** (35 total): `tst_spritestate2_undo_redo`
  (full do→undo→redo matrix for every operation type — `redo()`
  previously had zero coverage), `tst_load_truncated`,
  `tst_export_failures`, `tst_image_content_sniff` (the
  `validateImageFile` decoder sniff previously never executed in any
  test). Plus new cases in `tst_inputimage` (scale bounds),
  `tst_lvkanimation` (32-bit flags, position-stable swaps),
  `tst_path_traversal` (consecutive-dots names), and
  `tst_image_tab_controller` (frame-cascade regression).
- **`docs/json-export.md`**: schema reference for the JSON sprite-atlas
  export (frames hash, `animations` LVK extension, `meta`, limits) —
  previously referenced as "documented separately" but missing.
- **Populated i18n catalogs**: `translations/*.ts` now carry all 338
  extractable strings (they shipped with 0, so the embedded `.qm` files
  were empty and `installTranslator()` could never succeed). The
  English catalog is fully finished; es/fr are translator-ready
  skeletons. New manual CMake target `lvk_lupdate` refreshes them.

### CI

- `tidy-check` builds all `*_autogen` targets first (it failed
  deterministically on missing `ui_mainwindow.h`); `format-check` is
  green again (46 accumulated violations fixed).
- `release.yml`: build capped at `-j 2` (same documented OOM fix as
  `build.yml`), pushed tags validated against the CMake project
  version, upload steps tag-guarded so `workflow_dispatch` acts as a
  packaging dry-run, and `macdeployqt` bundles Qt into the `.app`
  before the DMG is packed.
- DEB install steps use the real `LvkSpriteEditor*` package glob (the
  old `lvksprite*` globs never matched); macOS lanes install `qttools`
  so translations and `macdeployqt` are available; archive lists
  aligned between workflows.

### Documentation

- README: `--format json|all` documented as fully functional (they
  were described as unwired), full preset list, exit codes, headless
  display fallback, corrected test count, and the round-trip guarantee
  restated as structural (matching the tests) rather than
  byte-for-byte.
- `docs/lvks-format.md`: corrected the version table (v0.2 = image
  scale + aframe ox/oy; v0.3 = animation flags + custom_header) and the
  animation field-count table; documented load-time validation gates
  and the canonical form; custom_header fixed-point behavior replaces
  the "known bug" note; stale line-number references replaced with
  function-level ones.
- `docs/keybindings.md`: documented the two-layer shortcut model
  (platform `keyBindings()` over `.ui` literals), the live View → Theme
  submenu (the doc claimed no menu existed and a restart was needed),
  and removed a reference to a nonexistent `replaceShortcutForMac()`.
- `docs/cocos2d-export.md`: include-guard derivation corrected (full
  output path, not basename); JSON pointer fixed.
- `UPGRADE_NOTES.md`: resolution markers brought up to date (items #2
  and #9 were fixed but unmarked; superseded fix descriptions
  corrected); stale FIXME markers in `spritestate.cpp` flipped to
  RESOLVED.
- Corrections to this changelog's own 2.0.x entries where they
  contradicted the shipped code (see the notes inline below).

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
- **10 new test executables** (21 at this phase; the Round 6 additions
  below brought the 2.0.1 total to 31):
  - `tst_version_preservation` — v0.1 round-trip + bump-on-edit
  - `tst_aframe_order` — aframe id-order canonicalisation (sorted at
    save; load preserves file order)
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
  switched `insert(id,…)` to `append(…)`). Restored via a SAVE-time
  `std::sort` by id (the load path preserves file order in memory;
  a freshly-loaded canonical file is therefore id-ordered). *[Correction:
  this entry originally said "load-time"; the shipped code sorts at
  save — see the Phase B1.3 comment in `SpriteState::load()`.]*
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
  png/jpg/jpeg/bmp/gif/webp/xpm/xbm/tif/tiff — ten formats; SVG is
  deliberately EXCLUDED to close XXE / scripted-SVG exposure (F1.3).
  `QImageReader::setAllocationLimit(256MB)` applied at startup.
  *[Correction: this entry originally listed svg as whitelisted.]*
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
  `windows`). Windows uses a non-Makefiles generator instead of the
  previously-hardcoded `Unix Makefiles` (which silently failed on
  `windows-latest`). *[Correction: originally `Visual Studio 17 2022`;
  the preset was later switched to single-config Ninja — see its
  description in `CMakePresets.json`.]*
- `build.yml` matrix per-OS preset; artifact upload path globs
  `build/**/LvkSpriteEditor*` to catch MSVC's `build/Release/`;
  `if-no-files-found: error`.
- `clang-format` dropped `continue-on-error: true`, pinned
  `clang-format-18`, recurses `find src -name '*.cpp' -o -name '*.h'`.
- New `clang-tidy` job with `compile_commands.json`.
- `CPACK_DEBIAN_PACKAGE_DEPENDS` set so `dpkg -i` resolves.
- `release.yml` (new) on tag push: cpack + `softprops/action-gh-release`.

### Changed (Round 6, Team H)

A follow-up 5-agent pass closing residual issues found post-2.0.1:

- **Critical: release runner / undo hangs / animation timer.**
  `release.yml` Linux runner switched `ubuntu-22.04` -> `ubuntu-latest`
  so apt's Qt6 satisfies `Qt6 6.4 REQUIRED` (22.04 ships 6.2.4; tag
  pushes were hard-failing at configure). `SpriteState2` transactions
  now depth-counted (nested start/end no longer push duplicate
  markers); `undo()`/`redo()` cap iteration at buffer size and warn
  out instead of infinite-looping when a transaction marker was
  evicted. `LvkAnimationWidget` clamps animation-frame delay to a
  16ms minimum so default `delay=0` aframes don't spin
  `startTimer(0)` and freeze the UI.
- **Atomic save + JSON exporter hardening.** `QSaveFile` resolves
  symlinks first so `save()` writes through the link instead of
  replacing it; `setDirectWriteFallback(true)` lets FAT32/SMB saves
  degrade gracefully. JSON exporter disambiguates duplicate frame
  names with a `_N` suffix (was silently overwriting). Atlas packing
  uses a stable sort with `frameId` tiebreak for deterministic output
  across libstdc++ versions. JSON+PNG export is best-effort atomic
  (PNG written first; on JSON failure the PNG is removed).
- **UI: theme menu, dialog polish, widget fixes.** View > Theme
  submenu (System / Light / Dark) actually wires the `Theme` system
  (was dead code; mode frozen at startup). `dialogs.cpp` helpers now
  take a parent widget, set window title and icon, and a new
  `errorDialog()` helper added; modal alerts center over `MainWindow`
  instead of floating disconnected. About dialog uses `Qt::RichText`
  explicitly so the fork URL and license link render correctly (was
  collapsing `\n\n` to a single space). `LvkInputImageWidget` Ctrl+wheel
  zoom now accepts the event instead of double-firing accept-then-ignore
  (was zooming AND scrolling the parent `QScrollArea`). `setPixmap`
  auto-invalidates the per-id cache so callers can't accidentally
  serve a stale scaled pixmap.
- **Docs honesty + transitions comment conditional.**
  `docs/lvks-format.md` drops the "byte-for-byte roundtrip" claim
  (the test does structural compare) and documents that
  `custom_header` lines have leading whitespace stripped (was claimed
  verbatim). `spritestate.cpp` `save()` only emits the
  `# transitions intentionally dropped` comment when `load()`
  actually encountered a `transitions(` block (was emitted
  unconditionally; mario roundtrip gained noise). New
  `docs/cocos2d-export.md` schema reference for the `.lkot` / `.lkob`
  / `AnimNameDef_*.h` trio.
- **Test coverage + CI honesty.** `release.yml` now runs `ctest`
  before packaging (was: only `--help` smoke). CPack OS string
  `Darwin` -> `macOS` for user-facing DMG name. New unit tests:
  `tst_cli_exit_codes` (documented 0/-1/2 exits),
  `tst_recent_files` (MRU dedup/order/overflow), `tst_geometry_persistence`
  (save+restore across `MainWindow` instances),
  `tst_undo_save_integration` (undo -> save -> reload preserves
  state); plus empty-`SpriteState` roundtrip coverage in
  `tst_lvks_roundtrip`.
- **`endTransaction()` underflow.** `Q_ASSERT(depth > 0)` replaced
  with `qWarning() + early return` so release and debug behave the
  same way and the no-op contract holds (H1's underflow test was
  aborting in debug).

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
