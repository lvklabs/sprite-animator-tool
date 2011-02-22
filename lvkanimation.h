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
    LvkAnimation(Id id = NullId, const QString& name = "");
    LvkAnimation(const QString& str);

    Id      id;         /* animation id */
    QString name;       /* animation name */

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
    { return id == ani.id && name == ani.name && _aframes == ani._aframes; }

    QList<LvkAframe> _aframes;
};

#endif // LVKANIMATION_H
