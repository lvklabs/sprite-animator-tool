// image_validation.cpp
//
// Team D2 (D2.1): see image_validation.h. Implementation lifted from
// ImageTabController::validateImageFile so that the loader path can
// share the same whitelist without dragging in Qt::Widgets / dialogs.
//
// Team F1 (F1.2 / F1.3): added ExistenceCheck mode (closes the load-time
// TOCTOU bypass) and dropped SVG from the whitelist (XXE + script
// surface that sprite assets do not need; see comment below). Whitelist
// here is now byte-identical to the GUI dialog filter in
// ImageTabController::addImageDialog -- F1.3 alignment.

#include "image_validation.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSet>

namespace lvk {

namespace {

// Team F1 (F1.3): one source of truth for the image-format whitelist.
// Used by both the extension probe (for absent-file mode) and the
// QImageReader content probe (when the file is on disk). Must stay in
// sync with the QFileDialog filter in
// ImageTabController::addImageDialog: png/jpg/jpeg/bmp/gif/webp/xpm/
// xbm/tif/tiff. SVG was dropped to close XXE / scripted-SVG attack
// surface -- a malicious SVG can reference external entities or fire
// JS-via-CSS in Qt's SVG renderer, and sprite assets have no need for
// vector formats. GIF and WebP, by contrast, are bitmap formats Qt6
// supports out of the box and the dialog already missed them (F1.3 fix).
const QSet<QByteArray> &allowedFormats() {
    static const QSet<QByteArray> allowed = {
        QByteArrayLiteral("png"),  QByteArrayLiteral("jpg"), QByteArrayLiteral("jpeg"),
        QByteArrayLiteral("bmp"),  QByteArrayLiteral("gif"), QByteArrayLiteral("webp"),
        QByteArrayLiteral("xpm"),  QByteArrayLiteral("xbm"), QByteArrayLiteral("tif"),
        QByteArrayLiteral("tiff"),
    };
    return allowed;
}

// Extract a lowercased extension from @p filename (no leading dot).
// Returns an empty QByteArray if there's no extension.
QByteArray lowerExtension(const QString &filename) {
    return QFileInfo(filename).suffix().toLower().toUtf8();
}

} // namespace

bool validateImageFile(const QString &filename, QString *errMsg, ExistenceCheck mode) {
    const bool fileExists = QFileInfo(filename).exists();

    if (!fileExists) {
        // Team F1 (F1.2): in Required mode the GUI path expects a real
        // file (the dialog only returns paths it picked from disk). In
        // Optional mode the load path tolerates broken asset links --
        // but we STILL must gate on the extension so a smuggled
        // ".eps" / ".pdf" / etc. record cannot bypass the whitelist by
        // deferring file creation until after load completes.
        if (mode == ExistenceCheck::Required) {
            if (errMsg) {
                *errMsg = QCoreApplication::translate("ImageValidation", "File '") + filename +
                          QCoreApplication::translate("ImageValidation", "' does not exist");
            }
            return false;
        }

        // Optional mode + absent file: extension-only check.
        const QByteArray ext = lowerExtension(filename);
        if (ext.isEmpty()) {
            if (errMsg) {
                *errMsg = filename + QCoreApplication::translate(
                                         "ImageValidation",
                                         " has no extension (and the file is not present "
                                         "for content-sniffing)");
            }
            return false;
        }
        if (!allowedFormats().contains(ext)) {
            if (errMsg) {
                *errMsg = filename +
                          QCoreApplication::translate("ImageValidation",
                                                      " has an unsupported image extension: ") +
                          QString::fromUtf8(ext);
            }
            return false;
        }
        return true;
    }

    // File exists -- run the full content-sniff path. We also still gate
    // on the extension so a file named "evil.eps" whose first bytes
    // happen to look like a PNG header (an attacker could craft this)
    // cannot bypass the whitelist via a sniffer-confusion attack: the
    // extension must ALSO be on the list.
    const QByteArray ext = lowerExtension(filename);
    if (!ext.isEmpty() && !allowedFormats().contains(ext)) {
        if (errMsg) {
            *errMsg = filename +
                      QCoreApplication::translate("ImageValidation",
                                                  " has an unsupported image extension: ") +
                      QString::fromUtf8(ext);
        }
        return false;
    }

    // SECURITY (Phase 4 + B3.1 expansion, refactored by D2.1): Whitelist
    // image formats by sniffing file content via QImageReader. This is the
    // SAME whitelist the dialog path used to enforce; we now enforce it
    // from BOTH paths so a malicious .lvks with a fake .eps cannot reach
    // SpriteState by skipping the dialog.
    QImageReader reader(filename);
    const QByteArray fmt = reader.format().toLower();
    if (!allowedFormats().contains(fmt)) {
        if (errMsg) {
            *errMsg = filename +
                      QCoreApplication::translate("ImageValidation",
                                                  " has an unsupported image format: ") +
                      QString::fromUtf8(fmt);
        }
        return false;
    }
    if (QImage(filename).isNull()) {
        if (errMsg) {
            *errMsg = filename +
                      QCoreApplication::translate("ImageValidation",
                                                  " has an invalid image format");
        }
        return false;
    }
    return true;
}

} // namespace lvk
