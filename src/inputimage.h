#ifndef INPUTIMAGE_H
#define INPUTIMAGE_H

#include <QPixmap>
#include <QString>

#include "types.h"

// Forward-declared so we don't drag the entire spritestate.h header into
// every translation unit that just wants InputImage.
enum class LvkVersion;

namespace lvk {

/// SECURITY: true if `path` is safe to hand to QPixmap()/QImage(): no
/// absolute path, Windows drive/UNC prefix, '..' traversal, '~' prefix, or
/// embedded NUL. Enforced on the .lvks load path (InputImage::fromString)
/// and on every GUI path that can introduce a filename, so a value the GUI
/// accepts is never rejected -- and silently dropped -- on the next load.
bool isSafeImagePath(const QString &path);

} // namespace lvk

/// Input image abstraction
struct InputImage {
    InputImage(Id id = NullId, const QString &filename = "", double scale = 1.0);
    InputImage(const QString &str);

    enum {
        PNG,
    };

    /// Bounds for the image scale factor. The upper bound mirrors the
    /// 8192px frame-dimension cap: a 4096px sheet at x16 already hits a
    /// 65536px pixmap; anything beyond is either a typo or an OOM/overflow
    /// attack via a hand-edited .lvks (see fromString()).
    static constexpr double kMinScale = 0.001;
    static constexpr double kMaxScale = 16.0;

    // TODO move this as private members
    Id id;            /* image id */
    QString filename; /* image filename */
    QPixmap pixmap;   /* image pixmap */

    /// returns the latest-version string representation (currently V_04).
    /// Equivalent to toString(LvkVersion::V_04).
    QString toString() const;

    /// Version-aware string representation. For v0.1 emits "id,filename"
    /// (no scale column); v0.2+ emits "id,filename,scale".
    QString toString(LvkVersion v) const;

    /// initializes the current instance from the string @param str
    bool fromString(const QString &str);

    /// force pixmap reload
    void reloadImage();

    /// free resources used by the image
    void freeImageData();

    /// set/get image scale
    void scale(double scale);
    double scale() const;

    /// operator ==
    bool operator==(const InputImage &img) const {
        return id == img.id && filename == img.filename && _scale == img._scale;
    }

private:
    double _scale; /* image scale */
};

#endif // INPUTIMAGE_H
