#include <QObject>
#include <QDebug>

#include "spritestate.h"
#include "undospritestate.h"

UndoSpriteState::UndoSpriteState(QObject* parent)
    : SpriteState(parent), _unsaved(false)
{
}

bool UndoSpriteState::undo()
{
    if (canUndo()) {
        StateChange st = _stBuffer.currentState();
        _stBuffer.prevState();

        qDebug() << "Undo" << st.type;

        switch (st.type) {

        /* basic mutable getters */
        case StateCircularBuffer::st_mutableGetImage:
            SpriteState::addImage(st.data.old_img);
            break;
        case StateCircularBuffer::st_mutableGetFrame:
            SpriteState::addFrame(st.data.old_frame);
            break;
        case StateCircularBuffer::st_mutableGetAnimation:
            SpriteState::addAnimation(st.data.old_ani);
            break;
        case StateCircularBuffer::st_mutableGetAframe:
            SpriteState::addAframe(st.data.old_aframe, st.data.ani.id);

        /* undo add */
        case StateCircularBuffer::st_addImage:
            SpriteState::removeImage(st.data.img.id);
            break;
        case StateCircularBuffer::st_addFrame:
            SpriteState::removeFrame(st.data.frame.id);
            break;
        case StateCircularBuffer::st_addAnimation:
            SpriteState::removeAnimation(st.data.ani.id);
            break;
        case StateCircularBuffer::st_addAframe:
            SpriteState::removeAframe(st.data.aframe.id, st.data.ani.id);
            break;

        /* undo remove */
        case StateCircularBuffer::st_removeImage:
            SpriteState::addImage(st.data.img);
            break;
        case StateCircularBuffer::st_removeFrame:
            SpriteState::addFrame(st.data.frame);
            break;
        case StateCircularBuffer::st_removeAnimation:
            SpriteState::addAnimation(st.data.ani);
            break;
        case StateCircularBuffer::st_removeAframe:
            SpriteState::addAframe(st.data.aframe, st.data.ani.id);
            break;

        default:
            break;
        }

        // TODO check if is saved.
        _unsaved = true;

        return true;
    }
    return false;
}

bool UndoSpriteState::redo()
{
    if (canRedo()) {
        _stBuffer.nextState();
        StateChange st = _stBuffer.currentState();

        qDebug() << "redo" << st.type;

        switch (st.type) {

        /* basic mutable getters */
        case StateCircularBuffer::st_mutableGetImage:
            SpriteState::addImage(st.data.img);
            break;
        case StateCircularBuffer::st_mutableGetFrame:
            SpriteState::addFrame(st.data.frame);
            break;
        case StateCircularBuffer::st_mutableGetAnimation:
            SpriteState::addAnimation(st.data.ani);
            break;
        case StateCircularBuffer::st_mutableGetAframe:
            SpriteState::addAframe(st.data.aframe, st.data.ani.id);

        /* redo add */
        case StateCircularBuffer::st_addImage:
            SpriteState::addImage(st.data.img);
            break;
        case StateCircularBuffer::st_addFrame:
            SpriteState::addFrame(st.data.frame);
            break;
        case StateCircularBuffer::st_addAnimation:
            SpriteState::addAnimation(st.data.ani);
            break;
        case StateCircularBuffer::st_addAframe:
            SpriteState::addAframe(st.data.aframe, st.data.ani.id);
            break;

        /* redo remove */
        case StateCircularBuffer::st_removeImage:
            SpriteState::removeImage(st.data.img.id);
            break;
        case StateCircularBuffer::st_removeFrame:
            SpriteState::removeFrame(st.data.frame.id);
            break;
        case StateCircularBuffer::st_removeAnimation:
            SpriteState::removeAnimation(st.data.ani.id);
            break;
        case StateCircularBuffer::st_removeAframe:
            SpriteState::removeAframe(st.data.aframe.id, st.data.ani.id);
            break;

        default:
            break;
        }

        // TODO check if is saved.
        _unsaved = true;

        return true;
    }
    return false;
}

bool UndoSpriteState::canUndo()
{
    return _stBuffer.hasPrevState();
}

bool UndoSpriteState::canRedo()
{
    return _stBuffer.hasNextState();
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

// Load, save, export ******************************************************

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
    StateChange st;
    st.type = StateCircularBuffer::st_mutableGetImage;
    st.data.old_img = _images[imgId];
    st.data.img = SpriteState::image(imgId);
    _stBuffer.addState(st);
    _unsaved = true;

    return SpriteState::image(imgId);
}

LvkFrame& UndoSpriteState::frame(Id frameId)
{
    StateChange st;
    st.type = StateCircularBuffer::st_mutableGetFrame;
    st.data.old_frame = _frames[frameId];
    st.data.frame = SpriteState::frame(frameId);
    _stBuffer.addState(st);
    _unsaved = true;

    return SpriteState::frame(frameId);
}

LvkAnimation& UndoSpriteState::animation(Id aniId)
{
    StateChange st;
    st.type = StateCircularBuffer::st_mutableGetAnimation;
    st.data.old_ani = _animations[aniId];
    st.data.ani = SpriteState::animation(aniId);
    _stBuffer.addState(st);
    _unsaved = true;

    return SpriteState::animation(aniId);
}

LvkAframe& UndoSpriteState::aframe(Id aniId, Id aframeId)
{
    StateChange st;
    st.type = StateCircularBuffer::st_mutableGetAframe;
    st.data.ani = _animations[aniId];
    st.data.old_aframe = _animations[aniId].aframes[aframeId];
    st.data.aframe = SpriteState::aframe(aniId, aframeId);
    _stBuffer.addState(st);
    _unsaved = true;

    return SpriteState::aframe(aniId, aframeId);
}

// add *********************************************************************

void UndoSpriteState::addImage(const InputImage& img)
{
    StateChange st;
    st.type = StateCircularBuffer::st_addImage;
    st.data.img = img;
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::addImage(img);
}

void UndoSpriteState::addFrame(const LvkFrame& frame)
{
    StateChange st;
    st.type = StateCircularBuffer::st_addFrame;
    st.data.frame = frame;
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::addFrame(frame);
}

void UndoSpriteState::addAnimation(const LvkAnimation& ani)
{
    StateChange st;
    st.type = StateCircularBuffer::st_addAnimation;
    st.data.ani = ani;
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::addAnimation(ani);
}

void UndoSpriteState::addAframe(const LvkAframe& aframe, Id aniId)
{
    StateChange st;
    st.type = StateCircularBuffer::st_addAframe;
    st.data.aframe = aframe;
    st.data.ani.id = aniId;
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::addAframe(aframe, aniId);
}

// remove ******************************************************************

void UndoSpriteState::removeImage(Id id)
{
    StateChange st;
    st.type = StateCircularBuffer::st_removeImage;
    st.data.img = _images[id];
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::removeImage(id);
}

void UndoSpriteState::removeFrame(Id id)
{
    StateChange st;
    st.type = StateCircularBuffer::st_removeFrame;
    st.data.frame = _frames[id];
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::removeFrame(id);
}

void UndoSpriteState::removeAnimation(Id id)
{
    StateChange st;
    st.type = StateCircularBuffer::st_removeAnimation;
    st.data.ani = _animations[id];
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::removeAnimation(id);
}

void UndoSpriteState::removeAframe(Id aframeId, Id aniId)
{
    StateChange st;
    st.type = StateCircularBuffer::st_removeAframe;
    st.data.aframe = _animations[aniId].aframes[aframeId];
    st.data.ani.id = aniId;
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::removeAframe(aframeId, aniId);
}
