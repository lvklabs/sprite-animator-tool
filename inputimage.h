#ifndef INPUTIMAGE_H
#define INPUTIMAGE_H

#include <QString>
#include <QPixmap>

#include "types.h"

/// Input image abstraction
struct InputImage
{
    InputImage(Id id = NullId, const QString& filename = "", double scale = 1.0);
    InputImage(const QString& str);

    // TODO move this as private members
    Id      id;         /* image id */
    QString filename;   /* image filename */
    QPixmap pixmap;     /* image pixmap */

    /// returns the string representation
    QString toString() const;

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
