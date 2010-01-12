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

    // hash getters ************************************************************

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

    /// get frame pixmaps hash
    const QHash<Id, QPixmap>& fpixmaps() const
    { return _fpixmaps; }

    // pixmap getters ***********************************************************

    /// get pixmap data from image @param imgId
    const QPixmap& ipixmap(Id imgId) /* const */
    { return (imgId != NullId) ? _images[imgId].pixmap : nullPixmap; }

    /// get pixmap data from frame @param frameId
    const QPixmap& fpixmap(Id frameId) /* const */
    { return (frameId != NullId) ? _fpixmaps[frameId] : nullPixmap; }

    // basic const getters ******************************************************

    /// get const input image by Id
    const InputImage& const_image(Id imgId) /* const */
    { return _images[imgId]; }

    /// get const frame by Id
    const LvkFrame& const_frame(Id frameId) /* const */
    { return _frames[frameId]; }

    /// get const animation by Id
    const LvkAnimation& const_animation(Id aniId) /* const */
    { return _animations[aniId]; }

    /// get const aframe by Id
    const LvkAframe& const_aframe(Id aniId, Id aframeId) /* const */
    { return _animations[aniId].aframes[aframeId]; }

    // update *******************************************************************

    /// update image
    void updateImage(const InputImage& img)
    {
        _images[img.id] = img;
        reloadFramePixmaps(img);
    }

    /// update frame
    void updateFrame(const LvkFrame& frame)
    {
        _frames[frame.id] = frame;
        reloadFramePixmap(frame);
    }

    /// update animation
    void updateAnimation(const LvkAnimation& ani)
    { _animations[ani.id] = ani; }

    /// update aframe
    void updateAframe(const LvkAframe& aframe, Id aniId)
    { _animations[aniId].aframes[aframe.id] = aframe; }

    // add *********************************************************************

    /// add new input image
    void addImage(const InputImage& img)
    {  _images.insert(img.id, img); }

    /// add new frame
    void addFrame(const LvkFrame& frame)
    {
        _frames.insert(frame.id, frame);
        reloadFramePixmap(frame);
    }

    /// add new animation
    void addAnimation(const LvkAnimation& ani)
    { _animations.insert(ani.id, ani); }

    /// add new aframe to the animation @param aniId
    void addAframe(const LvkAframe& aframe, Id aniId)
    { _animations[aniId].aframes.insert(aframe.id, aframe); }


    // remove ******************************************************************

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

    // Load, save, export ******************************************************-

    /// Errors
    typedef enum {
        ErrNone = 0,
        ErrNullFilename,
        ErrFileDoesNotExist,
        ErrCantOpenReadMode,
        ErrCantOpenReadWriteMode,
        ErrInvalidFormat,
    } SpriteStateError;

    /// save instance to @param filename
    /// NOTE: Input image filenames cannot contain the charater ',',
    ///       otherwise deserialize() will fail
    bool save(const QString& filename, SpriteStateError* err = 0);

    /// load instance from @param filename
    bool load(const QString& filename, SpriteStateError* err = 0);

    /// clear all hashes
    void clear();

    /// export sprite file @param filename.
    /// If @param outputDir is null, the sprite file directory is used.
    bool exportSprite(const QString& filename, const QString& outputDir = QString(),
                      SpriteStateError* err = 0) const;

    /// returns the error string of @param err
    static const QString& errorMessage(SpriteStateError err);


protected:

    /// null pixmap
    QPixmap nullPixmap;

    /// input images hash
    QHash<Id, InputImage>   _images;

    /// frames hash
    QHash<Id, LvkFrame>     _frames;

    /// animations hash
    QHash<Id, LvkAnimation> _animations;

    // TODO (?) move frame pixmap into the LvkFrame classs
    /// Frame pixmaps
    QHash<Id, QPixmap>      _fpixmaps;


    // TODO (?) move this method inside LvkFrame
    /// force reload frame pixmap
    void reloadFramePixmap(const LvkFrame& frame);

    /// force reload image pixmaps
    void reloadImagePixmaps();

    /// force reload frame pixmaps. If @param img is not null, then
    /// only reloads those frames using the image @param img
    void reloadFramePixmaps(const InputImage& img = InputImage());

};

typedef SpriteState::SpriteStateError SpriteStateError;

#endif // SPRITESTATE_H
