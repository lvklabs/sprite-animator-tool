// image_validation.h
//
// Team D2 (D2.1): centralised image-file validator used by BOTH the GUI
// path (ImageTabController::addImage) and the loader path
// (SpriteState::load).
//
// Background: the file-format whitelist used to be advisory in the GUI
// (an "unsupported format" dialog followed by an insertion) and entirely
// absent from the load path. So a malicious .lvks file with a record like
//
//     0,evil.eps,1
//
// survived InputImage::fromString's isSafeImagePath gate (it's a clean
// relative path with no NUL / no "..") and was inserted into SpriteState
// by SpriteState::load(), where it was then rendered by the controller's
// refreshTable -- the format whitelist was never consulted on this path.
//
// To close that hole we lift the validator out of ImageTabController as a
// free function so the loader can call it without dragging in the
// controller's Qt::Widgets dependencies. The GUI path still calls the
// same function (via the controller's now-thin shim), so the whitelist
// is enforced consistently across both routes.
//
// On the load path, a rejected image is SKIPPED (the record is dropped
// with a qWarning) rather than aborting the whole load. The user gets a
// partial sprite -- better than silent admission of attacker-controlled
// data.
//
// Team F1 (F1.2): TOCTOU bypass closed. The pre-F1 loader skipped the
// whitelist entirely when the referenced file didn't exist on disk,
// admitting any extension to state -- an attacker who shipped a .lvks
// referencing "evil.eps" (not present at load time) and then dropped
// evil.eps later got that file decoded on the next refresh. Validator
// now exposes an ExistenceCheck mode: when the file is absent we still
// gate on the EXTENSION (the path-derived suffix). The Qt format-sniff
// step is skipped only when the file is absent; the GUI/dialog path
// keeps full content-sniff semantics.

#ifndef LVK_IMAGE_VALIDATION_H
#define LVK_IMAGE_VALIDATION_H

#include <QString>

namespace lvk {

/// How strictly @p filename must exist on disk.
enum class ExistenceCheck {
    /// File MUST exist on disk; sniff content via QImageReader. This is
    /// the GUI path (a file picker can never return a path that doesn't
    /// exist) and the legacy validator behavior.
    Required,
    /// File MAY be absent. If present, fully validate (extension AND
    /// content sniff). If absent, validate the extension only -- enough
    /// to reject smuggled formats (e.g. "evil.eps") while tolerating the
    /// legitimate broken-asset-link case (a .lvks referencing
    /// "nonexistent.png" still admits the record, only the pixmap will
    /// be null). This is the load path: it must close the TOCTOU bypass
    /// where an attacker drops a malicious file AFTER load time.
    Optional,
};

/// Verify @p filename's image format is on the whitelist.
///
/// The whitelist matches the file-dialog filter
/// (png/jpg/jpeg/bmp/gif/webp/xpm/xbm/tif/tiff). Note that .svg is
/// NOT on the whitelist -- it is a renderer surface (Qt SVG plugin) we
/// don't need for sprite assets and was dropped to close XXE / scripted-
/// SVG exposure (Team F1.3).
///
/// @p mode controls behavior when the file is missing on disk.
///   - ExistenceCheck::Required: a missing file is a hard rejection.
///     Content is sniffed via QImageReader::format().
///   - ExistenceCheck::Optional: a missing file is admitted iff its
///     filename has a whitelisted extension. A present file is sniffed
///     in full (extension AND content). This is the load-path mode that
///     closes the TOCTOU bypass.
///
/// Returns true on success. On failure, optionally populates @p errMsg
/// with a user-facing reason string.
///
/// This function performs file I/O (one stat + one decode probe), so
/// callers on hot redraw paths should cache the result. The two
/// load-time call sites (GUI dialog -> ImageTabController::addImage and
/// SpriteState::load) only validate once per record.
bool validateImageFile(const QString &filename, QString *errMsg = nullptr,
                       ExistenceCheck mode = ExistenceCheck::Required);

} // namespace lvk

#endif // LVK_IMAGE_VALIDATION_H
