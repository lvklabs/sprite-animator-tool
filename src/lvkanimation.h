#ifndef LVKANIMATION_H
#define LVKANIMATION_H

#include <QString>
#include <QList>

#include "types.h"
#include "lvkaframe.h"

/// Animation abstraction. An animation basically consists in a name
/// and an ordered list of animation frames (aframes for short)
struct LvkAnimation
{
    LvkAnimation(Id id = NullId, const QString& name = "", unsigned flags = 0);
    LvkAnimation(const QString& str);

    // TODO move this as private members
    Id           id;         /* animation id */
    QString      name;       /* animation name */
    unsigned     flags;      /* animation flags */

    /// returns the string representation
    QString toString() const;

    /// initializes the current instance from the string @param str
    bool fromString(const QString& str);

    ///  get aframe with id aframeId
    LvkAframe& aframe(Id aframeId);

    /// add aframe
    void addAframe(const LvkAframe& aframe);

    /// remove aframe
    void removeAframe(Id aframeId);

    /// swap aframes
    void swapAframes(Id aframeId1, Id aframeId2);

    /// operator ==
    bool operator==(const LvkAnimation& ani) const
    { return id == ani.id && name == ani.name && _aframes == ani._aframes && flags == ani.flags; }

    QList<LvkAframe> _aframes;
};

#endif // LVKANIMATION_H
