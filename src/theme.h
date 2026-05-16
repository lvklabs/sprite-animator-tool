// SPDX-License-Identifier: GPL-3.0-or-later
//
// theme.h — Dark / light palette + system-theme detection for the
// LVK Sprite Animator. Authored by Agent 9 of the Qt 6 modernization.
//
// The 2010-era code shipped with no palette wiring at all (the host
// style decided everything). Modern users expect a dark mode that
// follows the desktop, so this module provides three explicit modes
// (Auto/Light/Dark) and persists the choice under QSettings key
// "ui/theme". The actual palette swap happens via QApplication::setPalette
// so it propagates to every widget without touching mainwindow.cpp.
//
// Qt 6.5 introduced QStyleHints::colorScheme() for first-class system
// theme detection, but the upstream environment for the upgrade pins
// Qt 6.4.2. The autoDetect() heuristic therefore falls back to checking
// the system-default QPalette's window-color brightness — see the
// implementation for the cutoff.

#ifndef THEME_H
#define THEME_H

#include <QString>

class QApplication;

namespace Theme {

/// User-selectable theme mode. Persisted as a lowercase string token
/// ("auto"/"light"/"dark") under QSettings key "ui/theme".
enum class Mode {
    Auto = 0, ///< follow the host desktop (default)
    Light,    ///< force the bundled light palette
    Dark,     ///< force the bundled dark palette
};

/// Apply the bundled dark palette to @p app. Safe to call repeatedly.
/// The palette is also propagated as the application style so every
/// existing widget (including those created before the call) repaints.
void applyDarkPalette(QApplication* app);

/// Apply the bundled light palette to @p app. Symmetric with
/// applyDarkPalette() — this is an explicit reset rather than a
/// "do nothing and trust the system default".
void applyLightPalette(QApplication* app);

/// Detect whether the host desktop is currently in dark mode and
/// apply the matching palette. The detection strategy is:
///   * Qt >= 6.5: QGuiApplication::styleHints()->colorScheme().
///   * Qt <  6.5: compare the brightness of the system default
///                QPalette's Window color. value() < 128 ==> dark.
/// The heuristic is documented because it can misfire on heavily
/// customised themes; users who want a deterministic outcome should
/// pick Mode::Light or Mode::Dark explicitly.
void autoDetect(QApplication* app);

/// Dispatch to the right apply* helper for @p mode.
void apply(QApplication* app, Mode mode);

/// Persist @p mode under QSettings key "ui/theme". The companion
/// loadFromSettings() reads it back. Both calls honour the
/// organisation / application names that main.cpp sets earlier on
/// startup, so settings land in the canonical per-user location.
void saveToSettings(Mode mode);

/// Read the previously-persisted theme mode, or Mode::Auto if none
/// has ever been stored. Tolerant of unknown / legacy values.
Mode loadFromSettings();

/// Parse a CLI / settings token ("auto", "light", "dark", any case)
/// into a Mode. Unknown tokens map to Mode::Auto.
Mode fromString(const QString& token);

/// Inverse of fromString(): canonical lowercase token for @p mode.
QString toString(Mode mode);

} // namespace Theme

#endif // THEME_H
