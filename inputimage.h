#ifndef INPUTIMAGE_H
#define INPUTIMAGE_H

#include <QString>

#include "types.h"

/// Input image abstraction
struct InputImage
{
    InputImage(Id id = NullId, const QString& filename = "");
    InputImage(const QString& str);

    Id      id;         /* image id */
    QString filename;   /* image filename */

    /// returns the string representation
    QString toString() const;

    /// initializes the current instance from the string @param str
    bool fromString(const QString& str);
};

#endif // INPUTIMAGE_H
