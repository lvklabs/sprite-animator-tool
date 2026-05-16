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

## Logged by later agents

(empty — append below as needed)
