#ifndef LVKANIMATION_H
#define LVKANIMATION_H

#include <QString>
#include <QHash>

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

    QHash<Id, LvkAframe> aframes;

    /// returns the string representation
    QString toString() const;

    /// initializes the current instance from the string @param str
    bool fromString(const QString& str);

    /// operator ==
    bool operator==(const LvkAnimation& a) const
    { return id == a.id && name == a.name && aframes == a.aframes; }
};

#endif // LVKANIMATION_H
