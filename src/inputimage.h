#ifndef INPUTIMAGE_H
#define INPUTIMAGE_H

#include <QString>
#include <QPixmap>

#include "types.h"

// Forward-declared so we don't drag the entire spritestate.h header into
// every translation unit that just wants InputImage.
enum class LvkVersion;

/// Input image abstraction
struct InputImage
{
    InputImage(Id id = NullId, const QString& filename = "", double scale = 1.0);
    InputImage(const QString& str);

    enum { PNG,  };

    // TODO move this as private members
    Id      id;         /* image id */
    QString filename;   /* image filename */
    QPixmap pixmap;     /* image pixmap */

    /// returns the latest-version string representation (currently V_04).
    /// Equivalent to toString(LvkVersion::V_04).
    QString toString() const;

    /// Version-aware string representation. For v0.1 emits "id,filename"
    /// (no scale column); v0.2+ emits "id,filename,scale".
    QString toString(LvkVersion v) const;

    /// initializes the current instance from the string @param str
    bool fromString(const QString& str);

    /// force pixmap reload
    void reloadImage();

    /// free resources used by the image
    void freeImageData();

    /// set/get image scale
    void scale(double scale);
    double scale() const;

    /// operator ==
    bool operator==(const InputImage& img) const
    { return id == img.id && filename == img.filename && _scale == img._scale; }

private:
    double  _scale;      /* image scale */
};

#endif // INPUTIMAGE_H
