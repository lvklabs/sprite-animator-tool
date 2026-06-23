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

#ifndef LVK_IMAGE_VALIDATION_H
#define LVK_IMAGE_VALIDATION_H

#include <QString>

namespace lvk {

/// Verify @p filename exists on disk and decodes as one of the formats
/// the .lvks editor knows about (png/jpg/jpeg/bmp/gif/webp/svg/xpm/xbm/
/// tif/tiff). Sniffs the file content via QImageReader -- the extension
/// is not trusted.
///
/// Returns true on success. On failure, optionally populates @p errMsg
/// with a user-facing reason string.
///
/// This function performs file I/O (one stat + one decode probe), so
/// callers on hot redraw paths should cache the result. The two
/// load-time call sites (GUI dialog -> ImageTabController::addImage and
/// SpriteState::load) only validate once per record.
bool validateImageFile(const QString &filename, QString *errMsg = nullptr);

} // namespace lvk

#endif // LVK_IMAGE_VALIDATION_H
