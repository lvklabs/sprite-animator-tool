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
    LvkAframe(Id id = NullId, Id frameId = NullId, int delay = 200, int ox = 0, int oy = 0,
              bool sticky = false);

    LvkAframe(const QString& str);

    // TODO move this as private members
    Id     id;            /* Aframe id */
    Id     frameId;       /* frame id */
    int    delay;         /* time delay in milliseconds */
    int    ox;            /* offset x */
    int    oy;            /* offset y */
    bool   sticky;        /* Sticky, i.e. display always */

    /// returns the string representation
    QString toString() const;

    /// initializes the current instance from the string @param str
    bool fromString(const QString& str);

    /// operator ==
    bool operator==(const LvkAframe& f) const
    {
        return id      == f.id      &&
               frameId == f.frameId &&
               ox      == f.ox      &&
               oy      == f.oy      &&
               delay   == f.delay   &&
               sticky  == f.sticky;
    }
};

#endif // LVKAFRAME_H
