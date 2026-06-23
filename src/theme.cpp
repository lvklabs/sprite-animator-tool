// SPDX-License-Identifier: GPL-3.0-or-later
//
// theme.cpp — implementation of the small dark/light palette wrapper
// described in theme.h. Kept intentionally compact: every palette role
// we set is one that the Qt6 widget styles actually consult, so a single
// QApplication::setPalette() call is enough to flip the whole UI.

#include "theme.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

namespace {

constexpr const char *kSettingsKey = "ui/theme";

/// Build the bundled dark palette. Colour choices were tuned against
/// the Fusion style (which we force in applyDarkPalette() so the
/// palette has predictable effect even on themes like windowsvista
/// that ignore custom palettes).
QPalette makeDarkPalette() {
    QPalette p;

    // Greyscale anchors. The "base" is 1-2 shades lighter than the
    // window so list widgets / line edits visually float above the
    // background instead of sinking into it.
    const QColor window(0x2b, 0x2b, 0x2b);
    const QColor windowText(0xe0, 0xe0, 0xe0);
    const QColor base(0x1e, 0x1e, 0x1e);
    const QColor altBase(0x35, 0x35, 0x35);
    const QColor button(0x3a, 0x3a, 0x3a);
    const QColor buttonText(0xe0, 0xe0, 0xe0);
    const QColor disabledText(0x80, 0x80, 0x80);
    const QColor highlight(0x42, 0x85, 0xf4); // soft blue
    const QColor highlightText(0xff, 0xff, 0xff);
    const QColor link(0x4f, 0xa3, 0xff);
    const QColor tooltipBg(0x50, 0x50, 0x50);

    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, windowText);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, altBase);
    p.setColor(QPalette::Text, windowText);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, buttonText);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, highlightText);
    p.setColor(QPalette::Link, link);
    p.setColor(QPalette::LinkVisited, link.darker(120));
    p.setColor(QPalette::ToolTipBase, tooltipBg);
    p.setColor(QPalette::ToolTipText, windowText);

    // Disabled-group overrides so greyed-out controls stay readable
    // against the darker background.
    p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    p.setColor(QPalette::Disabled, QPalette::Highlight, button.darker(110));

    return p;
}

/// Build an explicit light palette. We *could* call
/// QApplication::setPalette(QPalette()) and let the system defaults
/// kick in, but doing that doesn't reset roles we previously
/// over-wrote in the dark palette — so the user could end up with a
/// mostly-light UI with a stray dark highlight. Instead we set every
/// role we set in the dark variant.
QPalette makeLightPalette() {
    QPalette p;

    const QColor window(0xf0, 0xf0, 0xf0);
    const QColor windowText(0x20, 0x20, 0x20);
    const QColor base(0xff, 0xff, 0xff);
    const QColor altBase(0xe8, 0xe8, 0xe8);
    const QColor button(0xe0, 0xe0, 0xe0);
    const QColor buttonText(0x20, 0x20, 0x20);
    const QColor disabledText(0x90, 0x90, 0x90);
    const QColor highlight(0x33, 0x77, 0xdd);
    const QColor highlightText(0xff, 0xff, 0xff);
    const QColor link(0x1a, 0x73, 0xe8);
    const QColor tooltipBg(0xff, 0xff, 0xdc);

    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, windowText);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, altBase);
    p.setColor(QPalette::Text, windowText);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, buttonText);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, highlightText);
    p.setColor(QPalette::Link, link);
    p.setColor(QPalette::LinkVisited, link.darker(120));
    p.setColor(QPalette::ToolTipBase, tooltipBg);
    p.setColor(QPalette::ToolTipText, windowText);

    p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    p.setColor(QPalette::Disabled, QPalette::Highlight, button.darker(110));

    return p;
}

/// Heuristic system-dark detection for Qt < 6.5: take the default
/// QPalette's window colour and compare its HSV value channel against
/// 128. Anything below counts as "the desktop is dark"; anything at or
/// above counts as light. This matches what most other Qt apps do
/// while we wait for the colorScheme() API to ship in the distro Qt.
bool systemPrefersDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto *hints = QGuiApplication::styleHints()) {
        return hints->colorScheme() == Qt::ColorScheme::Dark;
    }
#endif
    // Fall back to brightness probe.
    const QPalette systemPalette;
    const QColor windowColor = systemPalette.color(QPalette::Window);
    return windowColor.value() < 128;
}

} // namespace

namespace Theme {

void applyDarkPalette(QApplication *app) {
    if (!app) {
        return;
    }
    // Fusion has predictable palette plumbing across platforms; the
    // native styles on Windows / macOS often ignore palette overrides
    // for things like menu backgrounds, which would leave a half-dark UI.
    if (auto *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app->setStyle(fusion);
    }
    app->setPalette(makeDarkPalette());
}

void applyLightPalette(QApplication *app) {
    if (!app) {
        return;
    }
    if (auto *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app->setStyle(fusion);
    }
    app->setPalette(makeLightPalette());
}

void autoDetect(QApplication *app) {
    if (!app) {
        return;
    }
    if (systemPrefersDark()) {
        applyDarkPalette(app);
    } else {
        applyLightPalette(app);
    }
}

void apply(QApplication *app, Mode mode) {
    switch (mode) {
    case Mode::Dark:
        applyDarkPalette(app);
        break;
    case Mode::Light:
        applyLightPalette(app);
        break;
    case Mode::Auto: // fallthrough
    default:
        autoDetect(app);
        break;
    }
}

void saveToSettings(Mode mode) {
    QSettings settings;
    settings.setValue(QString::fromLatin1(kSettingsKey), toString(mode));
}

Mode loadFromSettings() {
    QSettings settings;
    const QVariant raw = settings.value(QString::fromLatin1(kSettingsKey));
    if (!raw.isValid()) {
        return Mode::Auto;
    }
    return fromString(raw.toString());
}

Mode fromString(const QString &token) {
    const QString t = token.trimmed().toLower();
    if (t == QStringLiteral("light"))
        return Mode::Light;
    if (t == QStringLiteral("dark"))
        return Mode::Dark;
    return Mode::Auto;
}

QString toString(Mode mode) {
    switch (mode) {
    case Mode::Light:
        return QStringLiteral("light");
    case Mode::Dark:
        return QStringLiteral("dark");
    case Mode::Auto: // fallthrough
    default:
        return QStringLiteral("auto");
    }
}

} // namespace Theme
