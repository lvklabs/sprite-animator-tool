#include <QObject>

#include "spritestate.h"
#include "undospritestate.h"


class UndoSpriteState::StateChange
{

public:
    StateChange() : id(NullId), pImg(0) { }

private:
    enum StateChange_e {
        mutableGetImage = 1,
        mutableGetFrame,
        mutableGetAnimation,
        mutableGetAframe,

    };

    Id id;
    union {
        InputImage*   pImg;
        LvkFrame*     pFrame;
        LvkAnimation* pAni;
        LvkAframe*    pAframe;
    };

};

UndoSpriteState::UndoSpriteState(QObject* parent)
    : SpriteState(parent), _unsaved(false)
{
}


bool UndoSpriteState::undo()
{
    // TODO
    return false;
}

bool UndoSpriteState::redo()
{
    // TODO
    return false;
}

bool UndoSpriteState::canUndo()
{
    // TODO
    return false;
}

bool UndoSpriteState::canRedo()
{
    // TODO
    return false;
}

bool UndoSpriteState::hasUnsavedChanges()
{
    return _unsaved;
}

void UndoSpriteState::markAsSaved()
{
    _unsaved = false;
}

 /* inherited methods */

// Load, save, export ******************************************************-

bool UndoSpriteState::save(const QString& filename, SpriteStateError* err)
{
    bool success = SpriteState::save(filename, err);
    _unsaved = !success;

    return success;
}

bool UndoSpriteState::load(const QString& filename, SpriteStateError* err)
{
    bool success = SpriteState::load(filename, err);
    _unsaved = !success;

    return success;
}

void UndoSpriteState::clear()
{
    _unsaved = false;
    SpriteState::clear();
}

// basic mutable getters ****************************************************

InputImage& UndoSpriteState::image(Id imgId)
{
    _unsaved = true;
    return SpriteState::image(imgId);
}

LvkFrame& UndoSpriteState::frame(Id frameId)
{
    _unsaved = true;
    return SpriteState::frame(frameId);
}

LvkAnimation& UndoSpriteState::animation(Id aniId)
{
    _unsaved = true;
    return SpriteState::animation(aniId);
}

LvkAframe& UndoSpriteState::aframe(Id aniId, Id aframeId)
{
    _unsaved = true;
    return SpriteState::aframe(aniId, aframeId);
}

// add *********************************************************************

void UndoSpriteState::addImage(const InputImage& img)
{
    _unsaved = true;
    SpriteState::addImage(img);
}

void UndoSpriteState::addFrame(const LvkFrame& frame)
{
    _unsaved = true;
    SpriteState::addFrame(frame);
}

void UndoSpriteState::addAnimation(const LvkAnimation& ani)
{
    _unsaved = true;
    SpriteState::addAnimation(ani);
}

void UndoSpriteState::addAframe(const LvkAframe& aframe, Id aniId)
{
    _unsaved = true;
    SpriteState::addAframe(aframe, aniId);
}

// remove ******************************************************************

void UndoSpriteState::removeImage(Id id)
{
    _unsaved = true;
    SpriteState::removeImage(id);
}

void UndoSpriteState::removeFrame(Id id)
{
    _unsaved = true;
    SpriteState::removeFrame(id);
}

void UndoSpriteState::removeAnimation(Id id)
{
    _unsaved = true;
    SpriteState::removeAnimation(id);
}

void UndoSpriteState::removeAframe(Id aframeId, Id aniId)
{
    _unsaved = true;
    SpriteState::removeAframe(aframeId, aniId);
}

// misc ********************************************************************

void UndoSpriteState::reloadFramePixmap(const LvkFrame& frame)
{
    _unsaved = true;
    SpriteState::reloadFramePixmap(frame);
}

void UndoSpriteState::reloadImagePixmaps()
{
    _unsaved = true;
    SpriteState::reloadImagePixmaps();
}

void UndoSpriteState::reloadFramePixmaps(const InputImage& img)
{
    _unsaved = true;
    SpriteState::reloadFramePixmaps(img);
}
