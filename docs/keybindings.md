# LVK Sprite Animator — Keyboard Shortcuts

Catalog of every `QAction` shortcut declared in `src/mainwindow.ui`,
produced and verified by Agent 9 of the Qt 6 modernization (May 2026).

The shortcut text comes verbatim from the `<shortcut>` property on each
`<action>` element. Two-key chords are written `Ctrl+A, Ctrl+I`
(meaning press Ctrl+A then Ctrl+I) — that is Qt's own
`QKeySequence` notation.

On macOS, Qt's `qt_macKeyMap` translates `Ctrl` to the Command (⌘)
key automatically; see also the `replaceShortcutForMac()` helper in
`src/mainwindow.cpp` which renders the labels with the proper glyphs
(⌘ ⇧ ⌥ ↩) inside the UI.

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
| Undo         | `Ctrl+Z` |       |
| Redo         | `Ctrl+Y` | Standard on Windows / Linux; macOS users may expect `Ctrl+Shift+Z`. |
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

Dark / light mode is set via `QSettings` rather than a menu action
(menu wiring deferred to Agent 8's controller refactor). To change
the theme persistently:

```bash
# pick one
gsettings set <app-key>  # not applicable; we use QSettings
# Or edit ~/.config/LVK/LVK Sprite Animation Tool.conf and set
# ui/theme=dark   (or light, or auto)
```

Restart the application for the change to take effect.

## Hi-DPI

Qt 6 enables hi-DPI scaling automatically; no `AA_EnableHighDpiScaling`
attribute is required (and the attribute is a no-op in Qt 6 anyway).
The main window now sizes itself to ~80% of the available screen
geometry, with a 1204x768 floor that matches the legacy hardcoded
resize, so it works at both 1080p and 4K out of the box.

## Standard shortcuts considered but not added

| Shortcut         | Rationale for omission                                |
| ---------------- | ----------------------------------------------------- |
| `Ctrl+Shift+Z`   | Redo alternate; `Ctrl+Y` already bound and the user can pick either via Qt's `QKeySequence::Redo` if added later. |
| `Ctrl+P` (Print) | The tool has no print path. Adding would surface as a no-op. |
| `Ctrl+F` (Find)  | Tables already filter via the column headers; no global Find. |
| `Esc` (Cancel)   | Owned by individual dialogs / the QInputDialog stack. |

If you want any of these wired, edit `src/mainwindow.ui` (a single
`<shortcut>` element on the existing `<action>`) and rebuild — no
C++ change required.
