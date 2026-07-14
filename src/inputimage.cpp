#include <QDebug>
#include <QFileInfo>
#include <QStringList>

#include <cmath>

#include "inputimage.h"
#include "spritestate.h"

namespace lvk {

// SECURITY (Phase 4): Reject attacker-controlled image paths read from a
// .lvks file. The file format itself only ever ships relative filenames
// (see examples/*.lvks). An absolute path, UNC prefix, ".." traversal, or
// embedded NUL byte signals an attack: SMB credential leak on Windows
// (UNC), arbitrary filesystem probe ("/etc/passwd"), sandbox escape ("..").
//
// Returns true if `path` looks safe to pass to QPixmap()/QImage(). On
// rejection, the caller MUST mark its InputImage invalid so the rest of
// the load pipeline doesn't try to read the path anyway.
bool isSafeImagePath(const QString &path) {
    if (path.isEmpty()) {
        // Empty filenames are an existing legacy case (see
        // tst_inputimage::testFromStringEmptyFilename) -- not malicious,
        // not a path we'll dereference. Treat as safe; the consumer
        // still gets a null pixmap.
        return true;
    }
    if (path.contains(QChar('\0'))) {
        return false; // embedded NUL: truncation attack on C-string consumers
    }
    if (path.contains(QStringLiteral(".."))) {
        return false; // any '..' segment
    }
    if (path.startsWith(QStringLiteral("\\\\"))) {
        return false; // Windows UNC \\host\share -- SMB credential exfil
    }
    if (path.startsWith(QChar('~'))) {
        // Team B3: ~-prefixed paths are not expanded by Qt's QPixmap on
        // Linux, but the on-disk .lvks format only ever ships clean
        // relative paths (see examples/*.lvks). A leading '~' is either
        // shell-expansion bait (if the operator pastes the path into a
        // terminal) or simply ambiguous; reject both for portability.
        return false;
    }
    if (path.size() >= 2 && path.at(0).isLetter() && path.at(1) == QChar(':')) {
        // Team B3: Windows drive-absolute path (e.g. "C:\foo" or
        // "C:/foo"). QFileInfo::isAbsolute() on Linux returns false for
        // these, so the existing absolute check above lets them through
        // -- which breaks cross-platform .lvks portability and gives a
        // Windows operator an arbitrary-read primitive when they load a
        // Linux-built sprite.
        return false;
    }
    if (QFileInfo(path).isAbsolute()) {
        return false; // absolute paths are an arbitrary-read primitive
    }
    return true;
}

} // namespace lvk

InputImage::InputImage(Id id, const QString &filename, double scale)
    : id(id), filename(filename), pixmap(QPixmap(filename)), _scale(scale) {}

InputImage::InputImage(const QString &str) {
    if (!fromString(str)) {
        // TODO should throw an exception
    }
}

QString InputImage::toString() const {
    return toString(LvkVersion::V_04);
}

QString InputImage::toString(LvkVersion v) const {
    // Phase 6b (Item 26): CSV format cannot represent ',' or NUL in the
    // filename field; reject rather than silently corrupt the saved file.
    // (isSafeImagePath already rejects NUL on load; reject on save too so
    // the invariant is symmetric across both directions of the round-trip.)
    if (filename.contains(QLatin1Char(',')) || filename.contains(QChar('\0'))) {
        qWarning() << "InputImage::toString refusing to serialize filename containing comma or NUL:"
                   << filename;
        return QString();
    }

    // v0.1 schema had no `scale` column; everything else carries it.
    if (v < LvkVersion::V_02) {
        return QStringLiteral("%1,%2").arg(QString::number(id), filename);
    }
    return QStringLiteral("%1,%2,%3").arg(QString::number(id), filename, QString::number(_scale));
}

bool InputImage::fromString(const QString &str) {
    QStringList list = str.split(",");

    if (list.size() >= 2 && list.size() <= 3) {
        const QString candidateFilename = list.at(1);

        // SECURITY (Phase 4): Validate the candidate filename BEFORE we
        // store it or hand it to QPixmap. Rejection wipes id/filename to
        // their NullId/empty sentinels so the caller sees an unambiguously
        // invalid InputImage and the rest of the load pipeline (which
        // checks fromString's return) bails on this entry.
        if (!lvk::isSafeImagePath(candidateFilename)) {
            // Team B3 (Phase 6c): use qWarning rather than qDebug so the
            // message reaches the default Qt logging stream and is
            // visible to CLI users running headless (release builds
            // strip qDebug). The data is silently dropped from the
            // sprite otherwise, which is confusing to users opening a
            // sanitised .lvks that came from a malicious source.
            //
            // TODO (out of scope for B3): also surface a rejection count
            // to the GUI loader so the user sees an aggregated "N image
            // entries were rejected for unsafe filename" banner.
            qWarning() << "InputImage::fromString: rejected unsafe image path:"
                       << candidateFilename;
            id = NullId;
            filename = QString();
            _scale = 1.0;
            pixmap = QPixmap();
            return false;
        }

        // SECURITY: bound the scale factor. A hand-edited/malicious scale
        // (huge, non-positive, NaN/inf, or non-numeric) is rejected like an
        // unsafe path -- clamping instead would silently alter the sprite.
        double candidateScale = 1.0;
        if (list.size() >= 3) {
            bool scaleOk = false;
            candidateScale = list.at(2).toDouble(&scaleOk);
            if (!scaleOk || !std::isfinite(candidateScale) || candidateScale < kMinScale ||
                candidateScale > kMaxScale) {
                qWarning() << "InputImage::fromString: rejected out-of-range image scale:"
                           << list.at(2);
                id = NullId;
                filename = QString();
                _scale = 1.0;
                pixmap = QPixmap();
                return false;
            }
        }

        id = list.at(0).toInt();
        filename = candidateFilename;
        _scale = candidateScale;

        if (_scale == 1.0) {
            pixmap = QPixmap(filename);
        } else {
            scale(_scale);
        }

        if (pixmap.isNull()) {
            // Team B3: qWarning so a missing-on-disk asset reaches the
            // CLI user; this is the "silent data loss" half of B3.3.
            qWarning() << "InputImage::fromString: null pixmap loaded for file:" << filename;
        }
        return true;
    } else {
        qWarning() << "InputImage::fromString: invalid string format:" << str;
        return false;
    }
}

void InputImage::scale(double scale) {
    _scale = scale;

    QPixmap origPixmap(filename);
    if (!origPixmap.isNull()) {
        // Guard the double->int conversion: for out-of-range values the
        // conversion is undefined behavior, and even in-range results can
        // request absurd allocations. 65536px per side is already 4x the
        // 16384px atlas cap; refuse anything larger.
        const double w = origPixmap.width() * _scale;
        const double h = origPixmap.height() * _scale;
        constexpr double kMaxPixmapDim = 65536.0;
        if (!std::isfinite(w) || !std::isfinite(h) || w < 0 || h < 0 || w > kMaxPixmapDim ||
            h > kMaxPixmapDim) {
            qWarning() << "InputImage::scale: refusing scaled pixmap of" << w << "x" << h
                       << "for" << filename;
            pixmap = QPixmap();
            return;
        }
        pixmap = origPixmap.scaled(static_cast<int>(w), static_cast<int>(h), Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
    } else {
        pixmap = QPixmap();
    }
}

double InputImage::scale() const {
    return _scale;
}

void InputImage::reloadImage() {
    scale(_scale);
}

void InputImage::freeImageData() {
    // TODO check if this actually does something
    pixmap = QPixmap();
}
