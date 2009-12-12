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
class SpriteState : public QObject
{

public:
    SpriteState(QObject* parent = 0);

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
    { return _images[imgId].pixmap; }

    /// get pixmap data from frame @param frameId
    const QPixmap& fpixmap(Id frameId)
    { return _fpixmaps[frameId]; }

    /// get frame pixmaps hash
    const QHash<Id, QPixmap>& fpixmaps() const
    { return _fpixmaps; }

    /// get input image by Id
    InputImage& image(Id imgId)
    { return _images[imgId]; }

    /// get frame by Id
    LvkFrame& frame(Id frameId)
    { return _frames[frameId]; }

    /// get animation by Id
    LvkAnimation& animation(Id aniId)
    { return _animations[aniId]; }

    /// get aframe by Id
    LvkAframe& aframe(Id aniId, Id aframeId)
    { return _animations[aniId].aframes[aframeId]; }

    /// add new input image
    void addImage(const InputImage& img)
    {  _images.insert(img.id, img); }

    /// add new frame
    void addFrame(const LvkFrame& frame)
    {
        _frames.insert(frame.id, frame);
        reloadFramePixmap(frame);
    }

    // TODO move this method inside LvkFrame (?)
    /// force reload frame pixmap
    void reloadFramePixmap(const LvkFrame& frame)
    {
        QPixmap tmp(ipixmap(frame.imgId));
        QPixmap fpixmap(tmp.copy(frame.ox, frame.oy, frame.w, frame.h));
        _fpixmaps.insert(frame.id, fpixmap);
    }

    /// add new animation
    void addAnimation(const LvkAnimation& ani)
    { _animations.insert(ani.id, ani); }

    /// add new aframe to the animation @param aniId
    void addAframe(const LvkAframe& aframe, Id aniId)
    { _animations[aniId].aframes.insert(aframe.id, aframe); }

    /// remove input image by id
    void removeImage(Id id)
    { _images.remove(id); }

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

    /// force reload image pixmaps
    void reloadImagePixmaps();

    /// force reload frame pixmaps. If @param img is not null, then
    /// only reloads those frames using the image @param img
    void reloadFramePixmaps(const InputImage& img = InputImage());

    /// Errors
    typedef enum {
        ErrNone = 0,
        ErrNullFilename,
        ErrFileDoesNotExist,
        ErrCantOpenReadMode,
        ErrCantOpenReadWriteMode,
        ErrInvalidFormat,
    } SpriteStateError;

    /// serialize instance to @param filename
    /// NOTE: Input image filenames cannot contain the charater ',',
    ///       otherwise deserialize() will fail
    bool serialize(const QString& filename, SpriteStateError* err = 0) const;

    /// deserialize instance from @param filename
    bool deserialize(const QString& filename, SpriteStateError* err = 0);

    /// export binary sprite file
    bool exportSprite(const QString& filename, SpriteStateError* err = 0) const;

    /// returns the error string of @param err
    static const QString& errorMessage(SpriteStateError err);

private:
    /// input images hash
    QHash<Id, InputImage>   _images;

    /// frames hash
    QHash<Id, LvkFrame>     _frames;

    /// animations hash
    QHash<Id, LvkAnimation> _animations;

    // TODO move frame pixmap into the LvkFrame classs (?)
    /// Frame pixmaps
    QHash<Id, QPixmap>      _fpixmaps;
};

typedef SpriteState::SpriteStateError SpriteStateError;

#endif // SPRITESTATE_H
