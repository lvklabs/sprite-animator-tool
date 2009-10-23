#ifndef SPRITESTATE_H
#define SPRITESTATE_H

#include <QHash>
#include <QString>
#include <QImage>
#include <QPixmap>

#include "types.h"
#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"

/// The SpriteState class contains all the information about
/// input images, frames and animations
class SpriteState
{
public:
    SpriteState();

    /// get input images hash
    const QHash<Id, InputImage>& images() const
    { return _images; }

    /// get frames hash
    const QHash<Id, LvkFrame>& frames() const
    { return _frames; }

    /// get animations hash
    const QHash<Id, LvkAnimation>& animations() const
    { return _animations; }

    /// get aframes hash from animation @param aniId
    const QHash<Id, LvkAframe>& aframes(Id aniId) const
    { return _animations[aniId].aframes; }

    /// get pixmap data from image @param imgId
    const QPixmap& ipixmap(Id imgId)
    { return _ipixmaps[imgId]; }

    /// get pixmap data from frame @param frameId
    const QPixmap& fpixmap(Id frameId)
    { return _fpixmaps[frameId]; }

    /// add new input image
    void addImage(const InputImage& img)
    {
        _images.insert(img.id, img);
        _ipixmaps.insert(img.id, QPixmap(img.filename));
    }

    /// add new frame
    /// TODO use ox, oy, w and h
    void addFrame(const LvkFrame& frame)
    {
        _frames.insert(frame.id, frame);
        _fpixmaps.insert(frame.id, QPixmap(_ipixmaps[frame.imgId]));
    }

    /// add new animation
    void addAnimation(const LvkAnimation& ani)
    { _animations.insert(ani.id, ani); }

    /// add new aframe to the animation @param aniId
    void addAframe(const LvkAframe& aframe, Id aniId)
    { _animations[aniId].aframes.insert(aframe.id, aframe); }

    /// remove input image by id
    void removeImage(Id id)
    {
        _images.remove(id);
        _ipixmaps.remove(id);
    }

    /// remove frame by id
    void removeFrame(Id id)
    {
        _frames.remove(id);
        _fpixmaps.remove(id);
    }

    /// remove animation by id
    void removeAnimation(Id id)
    { _animations.remove(id); }

    /// remove aframe @param id in animation @param aniId
    void removeAframe(Id aframeId, Id aniId)
    { _animations[aniId].aframes.remove(aframeId); }

    /// clear all hashes
    void clear();

    /// Errors
    typedef enum {
        ErrCantOpenReadMode      = 1,
        ErrCantOpenReadWriteMode = 2,
        ErrInvalidFormat         = 3,
    } SpriteStateError;

    /// serialize instance to @param filename
    /// NOTE: Input image filenames cannot contain the charater ',',
    ///       otherwise deserialize() will fail
    bool serialize(const QString& filename, int* err = 0) const;

    /// deserialize instance from @param filename
    bool deserialize(const QString& filename, int* err = 0);

    //TODO write a displayAnimation function that takes the animation that needs to be displayed
    /// display preview of @param
    void displayAninamtion(const LvkAnimation& ani)
    {

    }

private:
    /// input images hash
    QHash<Id, InputImage>   _images;

    /// frames hash
    QHash<Id, LvkFrame>     _frames;

    /// animations hash
    QHash<Id, LvkAnimation> _animations;

    /// Input image pixmaps.
    QHash<int, QPixmap>     _ipixmaps;

    /// Frame pixmaps.
    QHash<int, QPixmap>     _fpixmaps;

    /// actual animations used to preview
    ///QHash<Id, LvkFrameGraphicsGroup> _gfanimation;
};

#endif // SPRITESTATE_H
