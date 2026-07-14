#ifndef LVKANIMATION_H
#define LVKANIMATION_H

#include <QList>
#include <QString>

#include "lvkaframe.h"
#include "types.h"

enum class LvkVersion;

/// Animation abstraction. An animation basically consists in a name
/// and an ordered list of animation frames (aframes for short)
struct LvkAnimation {
    LvkAnimation(Id id = NullId, const QString &name = "", unsigned flags = 0);
    LvkAnimation(const QString &str);

    // TODO move this as private members
    Id id;          /* animation id */
    QString name;   /* animation name */
    unsigned flags; /* animation flags */

    /// returns the latest-version (V_04) string representation.
    /// Equivalent to toString(LvkVersion::V_04).
    QString toString() const;

    /// Version-aware string representation. v0.1/v0.2 emit "id,name"
    /// (no flags column); v0.3+ emit "id,name,flags".
    QString toString(LvkVersion v) const;

    /// initializes the current instance from the string @param str
    bool fromString(const QString &str);

    /// true if an aframe with id aframeId exists in this animation
    bool hasAframe(Id aframeId) const;

    ///  get aframe with id aframeId
    LvkAframe &aframe(Id aframeId);

    /// @overload - const variant returns a const reference; needed for
    /// SpriteState::const_aframe() to be const-correct (Agent 7).
    const LvkAframe &aframe(Id aframeId) const;

    /// add aframe
    void addAframe(const LvkAframe &aframe);

    /// remove aframe
    void removeAframe(Id aframeId);

    /// swap aframes
    void swapAframes(Id aframeId1, Id aframeId2);

    /// operator ==
    bool operator==(const LvkAnimation &ani) const {
        return id == ani.id && name == ani.name && _aframes == ani._aframes && flags == ani.flags;
    }

    QList<LvkAframe> _aframes;
};

#endif // LVKANIMATION_H
