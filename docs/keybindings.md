# LVK Sprite Animator — Keyboard Shortcuts

Catalog of the `QAction` shortcuts. There are **two layers**:

1. `src/mainwindow.ui` declares a literal `<shortcut>` on each
   `<action>` element — those literals are what the tables below show.
2. After UI setup, the `MainWindow` constructor re-binds the seven
   standard actions (Open, Save, Save As, Undo, Redo, New/Close, Exit)
   to the **platform theme's** standard key list via
   `QKeySequence::keyBindings(...)`. When the platform defines
   bindings, they replace the `.ui` literal (this is how Redo also
   gains `Ctrl+Shift+Z` / `Alt+Shift+Backspace` on Linux, and how macOS
   gets ⌘-based keys); when the platform defines none (SaveAs and Quit
   on the generic Unix theme), the `.ui` literal is kept.

Everything else (chords, tab navigation, export) uses the `.ui`
literal verbatim. Two-key chords are written `Ctrl+A, Ctrl+I`
(meaning press Ctrl+A then Ctrl+I) — that is Qt's own
`QKeySequence` notation.

On macOS, Qt translates `Ctrl` in portable key sequences to the
Command (⌘) key automatically and renders menu labels with the proper
glyphs (⌘ ⇧ ⌥ ↩); no project code is involved.

## File menu

| Action          | Shortcut       | Notes                          |
| --------------- | -------------- | ------------------------------ |
| New (close current sprite) | `Ctrl+N` | Action is named `actionClose` for historical reasons but acts as "New". |
| Open file…      | `Ctrl+O`       |                                |
| Save            | `Ctrl+S`       |                                |
| Save as…        | `Ctrl+Shift+S` |                                |
| Export          | `Ctrl+E`       | Cocos2d `.lkob`/`.lkot`/`.h`. JSON atlas added by Agent 8. |
| Export as…      | `Ctrl+Shift+E` |                                |
| Exit            | `Ctrl+Q`       |                                |

## Edit menu

| Action       | Shortcut | Notes |
| ------------ | -------- | ----- |
| Undo         | `Ctrl+Z` | Plus platform alternates (e.g. `Alt+Backspace` on Linux). |
| Redo         | `Ctrl+Y` | Plus platform alternates: `Ctrl+Shift+Z` and `Alt+Shift+Backspace` on Linux, ⇧⌘Z on macOS. |
| Clear guides | *(none)* | Menu-only invocation.        |

## Tab navigation

| Action                    | Shortcut |
| ------------------------- | -------- |
| Frames tab                | `Alt+1`  |
| Animations tab            | `Alt+2`  |
| Show / hide frames preview| `Alt+3`  |

## Add / remove (two-key chords)

These follow the pattern `Ctrl+A` (add) or `Ctrl+R` (remove) followed by
a kind selector (`I` image, `F` frame, `A` animation, `U` unused).

| Action                | Shortcut       |
| --------------------- | -------------- |
| Add new image…        | `Ctrl+A, Ctrl+I` |
| Add new frame…        | `Ctrl+A, Ctrl+F` |
| Add new animation…    | `Ctrl+A, Ctrl+A` |
| Remove current image  | `Ctrl+R, Ctrl+I` |
| Remove current frame  | `Ctrl+R, Ctrl+F` |
| Remove current animation | `Ctrl+R, Ctrl+A` |
| Remove all unused frames | `Ctrl+R, Ctrl+U` |

`Add frame to current animation…` (`actionAddAframe`) and
`Remove frame from current animation` (`actionRemoveAframe`) have no
shortcut and are also hidden in the default UI; they remain in the
.ui only for completeness.

## Animation playback

| Action            | Shortcut |
| ----------------- | -------- |
| Refresh animation | `F5`     |

## Help

| Action      | Shortcut |
| ----------- | -------- |
| What's this | `F1`     |
| About…      | *(none)* |
| Invert animation frames order | *(none)* |

## Collision audit

No two actions share the same shortcut. The chorded
`Ctrl+A, Ctrl+A` for "Add new animation" intentionally reuses the
leader prefix and Qt resolves it without conflict because there is no
plain `Ctrl+A` action defined.

## Theme

Dark / light / system mode is switched live from the **View → Theme**
submenu (no restart required); the choice persists via `QSettings`
(`ui/theme` in `~/.config/LVK/LVK Sprite Animation Tool.conf` — the
same key can still be edited by hand if you prefer). See
`MainWindow::onThemeActionTriggered()` and `src/theme.cpp`.

## Hi-DPI

Qt 6 enables hi-DPI scaling automatically; no `AA_EnableHighDpiScaling`
attribute is required (and the attribute is a no-op in Qt 6 anyway).
The main window now sizes itself to ~80% of the available screen
geometry, with a 1204x768 floor that matches the legacy hardcoded
resize, so it works at both 1080p and 4K out of the box.

## Standard shortcuts considered but not added

| Shortcut         | Rationale for omission                                |
| ---------------- | ----------------------------------------------------- |
| `Ctrl+P` (Print) | The tool has no print path. Adding would surface as a no-op. |
| `Ctrl+F` (Find)  | Tables already filter via the column headers; no global Find. |
| `Esc` (Cancel)   | Owned by individual dialogs / the QInputDialog stack. |

If you want any of these wired, edit `src/mainwindow.ui` (a single
`<shortcut>` element on the existing `<action>`) and rebuild — no
C++ change required.
