# Upgrade Notes

This file tracks issues encountered during the 10-agent upgrade that the
discovering agent deferred to a later agent for proper resolution. Each
entry names the location, the issue, and the agent expected to address it.

## Logged by Agent 1 (Build System & Mechanical Qt6 Port)

The Phase 0 mechanical port left these compile-time warnings unresolved.
None of them block the build, but each requires a non-mechanical fix
(behavior change, refactor, or business-logic awareness) and so falls
outside Agent 1's "minimum to compile" scope.

### 1. `SpriteState::aframes(Id)` returns a dangling reference (real bug)

- **File:** `src/spritestate.h:42-43`
- **Warning:** `returning reference to temporary [-Wreturn-local-addr]`
- **Cause:** In Qt6 `QMap::operator[](key) const` returns the value **by
  value** (a copy), so `_animations[aniId]._aframes` yields a reference
  into a temporary that is destroyed at the end of the expression. The
  returned reference is dangling.
- **Suggested fix:** Either change the return type to `QList<LvkAframe>`
  (by value), or rewrite using `auto it = _animations.find(aniId);` and
  return `it.value()._aframes` from the iterator (whose value is stored
  in the map). The latter preserves the existing API.
- **Owner:** Agent 6 (Qt6 deeper API migration) or Agent 7 (memory safety)
  — the call is also a likely source of latent UB worth noting in the
  AddressSanitizer pass.
- **[RESOLVED by Agent 7]** Fixed with the const-iterator + static empty
  list approach (option b). Warning gone, test reverted.

### 2. `QMouseEvent::x()` / `QMouseEvent::y()` are deprecated

- **Files:**
  - `src/lvkanimationwidget.cpp:128-129`
  - `src/lvkframedefwidget.cpp:324-325`
  - `src/lvkinputimagewidget.cpp:259-260`
- **Warning:** `'int QMouseEvent::x() const' is deprecated: Use position()`
- **Suggested fix:** Replace `event->x()` / `event->y()` with
  `event->position().x()` / `event->position().y()` (returns `qreal`)
  and round/truncate to int as needed. The `position()` method returns
  `QPointF` in widget coordinates.
- **Owner:** Agent 6 (Qt6 deeper API migration).
- **[RESOLVED by Agent 6]** All three widgets use
  `event->position().toPoint()`; verified deprecation-clean by the
  Round 7 audit with `-DQT_DISABLE_DEPRECATED_BEFORE=0x060900`.

### 3. `QComboBox::activated(QString)` signal removed at runtime

- **Detected via:** Runtime warning when launching the executable —
  `qt.core.qobject.connect: QObject::connect: No such signal
   QComboBox::activated(QString) (sender name: 'previewScrSizeCombo',
   receiver name: 'MainWindow')`.
- **Cause:** Qt6 removed the `QString`-overload of `QComboBox::activated`;
  only `activated(int)` remains. The connect call is wired through the
  legacy `SIGNAL()`/`SLOT()` macros in `mainwindow.cpp`, so it compiles
  but fails silently at runtime.
- **Suggested fix:** Convert the affected connect to PMF
  (`QOverload<int>::of(&QComboBox::activated)`) **and** change the
  matching slot signature to take `int` (looking up the text via
  `comboBox->itemText(index)` if needed).
- **Owner:** Agent 6 (Qt6 deeper API migration — SIGNAL/SLOT to PMF
  conversion of all ~100 connections is the assigned scope).

## Logged by Agent 2 (Format Compatibility & Golden Tests)

### 4. `SpriteState::addAframe` inserts by id-as-index (heap corruption)

- **File:** `src/spritestate.cpp:88`
- **Symptom:** Loading any `.lvks` whose aframe ids are non-contiguous
  or do not start at 0 within an animation aborts with
  `malloc(): unaligned tcache chunk detected` (release) or
  `ASSERT failure in QList<T>::insert: "index out of range"` (debug).
  Both `examples/mario.lvks` and `examples/ryu.lvks` trigger this when
  loaded by the new test binaries — see
  `tests/format/tst_lvks_roundtrip.cpp::roundtripMario/Ryu`, which are
  `QSKIP`-ed pending the fix.
- **Cause:** `addAframe()` calls
  `_animations[aniId]._aframes.insert(aframe.id, aframe)` where
  `_aframes` is `QList<LvkAframe>`. `QList::insert(int i, const T&)`
  treats `i` as a position, not a key, and `i > size()` is undefined
  behavior. Under Qt5 the array-of-pointers QList sometimes hid the UB;
  the Qt6 unified `QList` (an array container) corrupts the heap. The
  bug has existed since the original Qt4 commits.
- **Suggested fix:** Replace the call with `append(aframe)` (or
  `push_back`). Aframe order is already determined by the order the
  parser encounters them in the file, and `save()` iterates the list
  in insertion order, so `append` preserves on-disk order without
  needing the id-as-position invariant. Cross-check
  `LvkAnimation::addAframe` (which already uses `push_back`) for the
  intended semantics.
- **Owner:** Agent 6 (Qt6 deeper API migration) or Agent 7 (memory
  safety / AddressSanitizer pass). Once fixed, drop the two `QSKIP`s
  in `tests/format/tst_lvks_roundtrip.cpp` so mario/ryu golden tests
  actually run.
- **[RESOLVED by Agent 7]** Replaced `insert(id, aframe)` with
  `append(aframe)`. QSKIPs removed; mario / ryu round-trip subtests now
  PASS.

### 5. `custom_header` round-trip grows trailing newlines

- **File:** `src/spritestate.cpp:158-160`
- **Symptom:** Each save / load cycle of a sprite that has a non-empty
  `custom_header()` block appends an extra `\n` to
  `SpriteState::_customHeader`. The canonical on-disk form is therefore
  not a fixed point — repeated save/load slowly grows the header.
- **Cause:** `load()` already appends `"\n"` after every header line
  (`spritestate.cpp:347`), so `_customHeader` always ends in `\n`.
  `save()` then writes `_customHeader << "\n"` (line 159), adding a
  second trailing newline. On reload, load() once again appends `\n`
  per line — including for the now-empty trailing line — so the
  invariant breaks by one newline per round.
- **Suggested fix:** In `save()`, write `_customHeader` *without* the
  extra `"\n"`, or call `_customHeader.trimmed()` before serialisation.
  The test `tst_lvks_roundtrip::roundtripPreservesCustomHeader` is
  written to tolerate this until the fix lands (compares with trailing
  whitespace stripped).
- **Owner:** Agent 8 (MainWindow refactor + export pipeline) owns
  `src/spritestate.cpp` export-side edits. Once fixed, tighten the
  comparison in `roundtripPreservesCustomHeader` back to a strict
  `QCOMPARE`.

## Logged by later agents

### Agent 3 (Test Infrastructure)

The dangling-reference bug Agent 1 logged in `SpriteState::aframes(Id)` was
reproduced empirically while writing `tests/unit/tst_spritestate_crud.cpp`:
calling `s.aframes(aniId).size()` segfaulted inside the test process.
The test was rewritten to read via `s.animations().value(aniId)._aframes`
(which goes through `QMap::value`, not `operator[] const`) so the test
suite does not depend on the latent bug being fixed first. Once Agent 6/7
land the fix, the test can be simplified back to `s.aframes(aniId)`.

## Agent 6 findings (Qt6 Deeper API Migration)

### 6. `QComboBox::activated(QString)` -> `activated(int) + itemText()` semantic risk

- **Site:** originally `src/mainwindow.cpp` (`ui->previewScrSizeCombo`);
  the connect now lives in `src/controllers/AnimationTabController.cpp`
  after the Agent 8 refactor.
- **Context:** The Qt4/Qt5 code wired `SIGNAL(activated(QString))` to
  `changePreviewScrSize(const QString&)`. Qt6 removed that overload entirely;
  only `activated(int)` remains. Agent 6 converted the connect to
  `QOverload<int>::of(&QComboBox::activated)` and recovers the text via
  `ui->previewScrSizeCombo->itemText(index)` inside a lambda, preserving the
  slot signature.
- **Why `activated`, not `currentTextChanged`:** `changePreviewScrSize()`
  pops up a modal `QInputDialog` when the user picks "Custom..." (line
  1530-ish). Switching to `currentTextChanged` would also fire when the combo
  is populated programmatically (e.g. when a custom resolution is added by
  `addItem(res)` at line 1583), which would re-trigger the dialog and create
  an infinite loop. `activated` only fires on user interaction, matching the
  legacy behavior.
- **Status:** Resolved by Agent 6. Listed here as documentation for the
  semantics shift; no further work needed unless behavior reports surface.

### 7. `QAction::triggered()` no-arg overload superseded by `triggered(bool=false)`

- **Sites:** ~25 `&QAction::triggered` connections in `src/mainwindow.cpp`
  and 1 in `src/lvkaction.cpp`.
- **Context:** Qt6's `QAction` declares a single `triggered(bool checked = false)`
  signal; the no-arg `triggered()` of the SIGNAL/SLOT era is just that signal
  invoked with the default. Agent 6 bound zero-arg slots directly via PMF
  (Qt auto-discards the trailing `bool`), and used `[this](bool){ ... }`
  lambdas where the slot has its own default arguments (`addFrameDialog`,
  `incAniSpeed`, `decAniSpeed`, `saveFile`/`saveAsFile`) so the slot's
  default-value path is taken rather than the `bool->int`/`bool->QString`
  implicit conversion.
- **Status:** Resolved. No risk.

### 8. `LvkAction::triggered(const QString&)` overload selection

- **Site:** `src/mainwindow.cpp:798-801` and `src/lvkaction.cpp:8`.
- **Context:** `LvkAction` redeclares a `triggered(const QString&)` signal
  on top of `QAction::triggered(bool)`. Name lookup in the derived class
  scope hides the base-class member, so unqualified `&LvkAction::triggered`
  is ambiguous to PMF; we explicitly select with
  `QOverload<const QString&>::of(&LvkAction::triggered)` for the
  `addRecentFileMenu` wiring, and bind via `&QAction::triggered` in the
  constructor of `LvkAction` itself.
- **Status:** Resolved.

### 9. `MainWindow::about()` is called from the constructor and blocks `--version`/`--help`

- **Site:** `src/mainwindow.cpp:188` (in `MainWindow::MainWindow`).
- **Symptom:** Running `LvkSpriteEditor --version` or `--help` does not
  print anything and never exits, because the modal About dialog is shown
  inside the constructor (`about()` -> `msg.exec()`), and `parseCmdLine`
  is only invoked after the constructor returns (`src/main.cpp:26-28`).
- **Suggested fix:** Move the `about()` call out of the constructor (e.g.
  defer it to the first event-loop iteration via `QTimer::singleShot(0,...)`,
  or, better, only show it on first launch tracked by `QSettings`).
  Independently, parse the CLI _before_ constructing `MainWindow` so that
  `--help`/`--version` never instantiate the GUI at all.
- **Owner:** Agent 10 (CLI modernization with `QCommandLineParser`) is the
  natural place; Agent 9 (UX/Accessibility) may want to delete the splash
  About entirely.
- **[RESOLVED by Agent 10]** `main()` parses the CLI before any
  `MainWindow` is constructed (`--version`/`--help`/`--export` never
  touch the GUI), and the About splash is gated behind a
  first-launch `QSettings` flag (`ui/showAboutOnStartup`). Pinned by
  `tests/unit/tst_cli_exit_codes.cpp`.

### 10. `QApplication::keyboardModifiers()` is still fine in Qt6

- **Sites:** `src/lvkinputimagewidget.h:113-120` (the `ctrlKey()`,
  `shiftKey()`, `altKey()` helpers). Audited while reviewing event-handler
  changes -- the static method is not deprecated in Qt6.
- **Status:** No action required.

## Agent 7 follow-up: Agent 3's workaround resolved

**[RESOLVED by Agent 7]** — `aframes(Id)` now uses a const-iterator lookup
with a static empty-list fallback, so the returned reference is always
valid. The test was reverted back to `s.aframes(aniId)`.

## Agent 7 findings (Modern C++ & Memory Safety)

### 1. `SpriteState::const_image/_frame/_animation/_aframe(Id)` had latent UB

When marking these getters `const` we found their previous bodies all used
`_collection[id]` (i.e. `QMap::operator[]`). On the non-const overload,
that has the side effect of *creating a default-constructed entry* if the
key is missing — so a "read-only" const_image(Id) lookup on a missing id
was actually mutating the map, growing it by one entry every call. The
fix uses `constFind()` + static null sentinel; no mutation, no dangling
reference.

### 2. `LvkAction` leaked on every recent-file menu rebuild

`MainWindow::addRecentFileMenu()` did `new LvkAction(filename)` with no
parent, then `QMenu::addAction(QAction*)` -- which does **not** take
ownership. The MAX_RECENT_FILES entries leaked one LvkAction each rebuild.
Agent 7 parented them to `this` (MainWindow); a 2.0.1 follow-up
re-parented them to the menu (`ui->actionOpenRecent`) because
`QMenu::clear()` only deletes menu-parented actions, so
MainWindow-parented ones still accumulated until window destruction.

### 3. `_pCache[1000][7]` raw owning array → `QCache<QPair<Id,int>,QPixmap>`

The previous LvkInputImageWidget allocated up to 7000 `QPixmap*` slots in
a stack array, with a hand-coded delete-loop in two places. Replaced
with QCache keyed by `(cacheId, zoomLevel)`. As a side effect:
- LRU eviction means we no longer keep cold zoomed pixmaps around forever.
- `useCacheId >= PCACHE_ROW_SIZE` is no longer a fatal bounds error.
- Net ~25 LOC removed.

### 4. Bug #4 was real heap corruption, not just an assert

The fix landed (`append` instead of `insert(id,...)`). The two `QSKIP`
markers in `tests/format/tst_lvks_roundtrip.cpp` were removed and the
mario / ryu round-trip subtests now PASS rather than skip.


## Agent 8 findings (MainWindow Refactor + JSON Atlas Export)

### Bug #5 RESOLVED: `custom_header` trailing-newline growth

`SpriteState::save()` now strips trailing whitespace from `_customHeader`
and writes exactly one '\n' terminator inside the `custom_header()`
block. `load()` already appends `"\n"` per line, so the canonical form
is now a fixed point and round-trips strictly. The `QSKIP`/tolerance in
`tests/format/tst_lvks_roundtrip.cpp::roundtripPreservesCustomHeader`
has been replaced with a strict `QCOMPARE`, plus a second save+reload
to verify fixed-point convergence.

### Agent 5 [FIXME] RESOLVED: path-traversal in `exportSprite()`

Added `isSafeExportPath()` in `src/spritestate.cpp` that rejects:
- empty baseName,
- baseName containing '/' / '\\' or '..',
- baseName equal to "." or "..",
- absolute baseName,
- outputDir that does not exist or cannot be canonicalised,
- resolved path that escapes the canonical outputDir.

A new SpriteStateError code `ErrUnsafeOutputPath` surfaces the
rejection. Covered by `tests/format/tst_path_traversal.cpp`.

### Refactor notes

- `src/mainwindow.cpp` shrank from 2,528 LOC to 590 LOC. Five
  thin controllers in `src/controllers/` own the per-tab logic:
  `ImageTabController`, `FrameTabController`, `AnimationTabController`,
  `TransitionTabController`, `ExportController`.
- `SpriteState::exportSprite` gained an `ExportFormat` enum
  (`Cocos2d | Json | All`) with a backwards-compat 4-arg overload so
  Agent 10's main.cpp keeps compiling until it adopts the new
  signature.
- A new `JsonAtlasExporter` in `src/exporters/` emits a packed PNG
  sheet (shelf-pack) + TexturePacker-compatible JSON descriptor with
  an extra `animations` key (LVK extension).
- `MainWindow::cellChangedSignals(bool)` was deleted entirely in
  Phase 3 (the post-refactor stub was an empty no-op that wiped the
  undo stack on every file open). Each controller now wraps its
  `setText()` calls with a per-controller `QSignalBlocker` (RAII),
  which mirrors the legacy bottleneck without the global side effect.

## Round 7 (validation audit, 2026-07)

A nine-dimension audit of the whole upgrade — Qt6 API completeness,
ASan/UBSan, test-coverage gaps, deep correctness review, documentation
drift, CI health, runtime smoke tests, i18n, and a cross-check of every
resolution claim in this file — followed by fixes for everything the
audit confirmed. See the `[Unreleased]` section of `CHANGELOG.md` for
the full fix list. Verification results worth recording here:

- **Deprecation-clean:** an independent Debug build of all translation
  units with `-DQT_DISABLE_DEPRECATED_BEFORE=0x060900 -Wall -Wextra
  -Wdeprecated-declarations` produced zero warnings; no legacy
  SIGNAL/SLOT, QMouseEvent::x/y, QRegExp, Q_FOREACH, etc. remain.
- **Sanitizer-clean:** ASan+UBSan over the full test suite plus real
  headless exports reported zero leaks and, after the
  `_resizingRect` fix, zero UB diagnostics.
- Every RESOLVED claim in this file checked out against the code; the
  markers missing on items #2 and #9 were added, and two superseded
  fix descriptions were corrected in place.

### Known items deliberately left open

1. **Sentence-fragment `tr()` concatenation** (~20 call sites, e.g.
   `tr("Are you sure you want to remove the image '") + name +
   tr("'?")`): correct translation is impossible for languages with
   different word order. Fix is mechanical (`tr("... '%1'?").arg(...)`)
   but touches many user-visible strings; batch it with a real
   translation pass over the es/fr catalogs.
2. **CLI help text is untranslatable** (QStringLiteral option
   descriptions in `src/main.cpp`); wrap in
   `QCoreApplication::translate` when i18n becomes a priority.
3. **es/fr catalogs are unfinished skeletons**: they carry the full
   338-string inventory (see `lvk_lupdate` target) but no translations.
4. **Windows CI lanes disabled**: per-target AUTOUIC generates
   duplicate `ui_mainwindow.h` rules under Ninja; needs the
   .ui-consuming code factored into a shared CMake library.
5. **GitHub Actions pinned by mutable tags** (`@v4` etc.) in a
   `contents: write` release workflow; consider SHA-pinning.
6. **`LvkAnimation::aframe(Id)` still returns a writable static
   sentinel** on lookup misses. All in-repo callers are now guarded
   (`hasAframe()` checks in SpriteState/SpriteState2), but the API
   itself remains a foot-gun for new code; consider an
   iterator/optional-style replacement.
