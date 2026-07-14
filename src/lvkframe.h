#ifndef LVKFRAME_H
#define LVKFRAME_H

#include <QRect>
#include <QString>

#include "types.h"

/// Frame abstraction.
/// ox, oy, w, and h are intended for future use to create
/// frames that are a portion of the original input image.
struct LvkFrame {
    LvkFrame(Id id = NullId, Id imgId = NullId, int ox = 0, int oy = 0, int w = 0, int h = 0,
             const QString &name = "");

    LvkFrame(const QString &str);

    /// Maximum frame width/height accepted by the loader (largest texture
    /// dimension shipping GPUs reliably support). GUI edit paths enforce
    /// the same bound so a value accepted in-session is never rejected --
    /// and silently dropped -- on the next load.
    static constexpr int kMaxDim = 8192;

    // TODO move this as private members
    Id id;        /* frame id */
    Id imgId;     /* input image id */
    int ox;       /* offset x */
    int oy;       /* offset y */
    int w;        /* width */
    int h;        /* height */
    QString name; /* frame name */

    /// returns the string representation
    QString toString() const;

    /// initializes the current instance from the string @param str
    bool fromString(const QString &str);

    /// returns a QRect with the values ox, oy, w and h
    QRect rect() const;

    /// sets ox, oy, w, and h from a QRect
    void setRect(const QRect &rect);

    /// operator ==
    bool operator==(const LvkFrame &f) const {
        return id == f.id && imgId == f.imgId && ox == f.ox && oy == f.oy && w == f.w && h == f.h &&
               name == f.name;
    }
};

#endif // LVKFRAME_H
