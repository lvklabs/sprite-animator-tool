#ifndef LVKAFRAME_H
#define LVKAFRAME_H

#include <QString>

#include "types.h"

/// Aframe means Animation frame. Consider an animation frame as an
/// ordinary frame with a time delay. We do not replicate all the
/// information that contains a frame (ox, oy, w, h, etc.), instead
/// we only store the frame Id
struct LvkAframe
{
    LvkAframe(Id id = NullId, Id frameId = NullId, double delay = 1.0);

    LvkAframe(const QString& str);

    Id     id;            /* Aframe id */
    Id     frameId;       /* frame id */
    double delay;         /* time delay in seconds */

    /// returns the string representation
    QString toString() const;

    /// initializes the current instance from the string @param str
    bool fromString(const QString& str);
};

#endif // LVKAFRAME_H
