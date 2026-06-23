// image_validation.cpp
//
// Team D2 (D2.1): see image_validation.h. Implementation lifted from
// ImageTabController::validateImageFile so that the loader path can
// share the same whitelist without dragging in Qt::Widgets / dialogs.

#include "image_validation.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSet>

namespace lvk {

bool validateImageFile(const QString &filename, QString *errMsg) {
    if (!QFileInfo(filename).exists()) {
        if (errMsg) {
            *errMsg = QCoreApplication::translate("ImageValidation", "File '") + filename +
                      QCoreApplication::translate("ImageValidation", "' does not exist");
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
    static const QSet<QByteArray> allowed = {
        QByteArrayLiteral("png"), QByteArrayLiteral("jpg"),  QByteArrayLiteral("jpeg"),
        QByteArrayLiteral("bmp"), QByteArrayLiteral("gif"),  QByteArrayLiteral("webp"),
        QByteArrayLiteral("svg"), QByteArrayLiteral("xpm"),  QByteArrayLiteral("xbm"),
        QByteArrayLiteral("tif"), QByteArrayLiteral("tiff"),
    };
    if (!allowed.contains(fmt)) {
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
