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

#ifdef DEBUG_UNDO
        qDebug() << "--- Undo ---";
#endif
        StateChange st = _stBuffer.currentState();
        _stBuffer.prevState();

        switch (st.type) {

        /* undo update */
        case StateCircularBuffer::st_updateImage:
            SpriteState::updateImage(st.data.old_img);
            break;
        case StateCircularBuffer::st_updateFrame:
            SpriteState::updateFrame(st.data.old_frame);
            break;
        case StateCircularBuffer::st_updateAnimation:
            SpriteState::updateAnimation(st.data.old_ani);
            break;
        case StateCircularBuffer::st_updateAframe:
            SpriteState::updateAframe(st.data.old_aframe, st.data.ani.id);

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

#ifdef DEBUG_UNDO
        qDebug() << "--- Redo --- ";
#endif
        _stBuffer.nextState();
        StateChange st = _stBuffer.currentState();

        switch (st.type) {

        /* redo update */
        case StateCircularBuffer::st_updateImage:
            SpriteState::updateImage(st.data.img);
            break;
        case StateCircularBuffer::st_updateFrame:
            SpriteState::updateFrame(st.data.frame);
            break;
        case StateCircularBuffer::st_updateAnimation:
            SpriteState::updateAnimation(st.data.ani);
            break;
        case StateCircularBuffer::st_updateAframe:
            SpriteState::updateAframe(st.data.aframe, st.data.ani.id);

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

    if (success) {
        _stBuffer.clear();
    }

    return success;
}

void UndoSpriteState::clear()
{
    _unsaved = false;
    _stBuffer.clear();
    SpriteState::clear();
}

// update *******************************************************************

void UndoSpriteState::updateImage(const InputImage& img)
{
    if (img == _images[img.id]) {
        return;
    }

    StateChange st;
    st.type = StateCircularBuffer::st_updateImage;
    st.data.old_img = _images[img.id];
    st.data.img = img;
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::updateImage(img);
}

void UndoSpriteState::updateFrame(const LvkFrame& frame)
{
    if (frame == _frames[frame.id]) {
        return;
    }

    StateChange st;
    st.type = StateCircularBuffer::st_updateFrame;
    st.data.old_frame = _frames[frame.id];
    st.data.frame = frame;
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::updateFrame(frame);
}

void UndoSpriteState::updateAnimation(const LvkAnimation& ani)
{
    if (ani == _animations[ani.id]) {
        return;
    }

    StateChange st;
    st.type = StateCircularBuffer::st_updateAnimation;
    st.data.old_ani = _animations[ani.id];
    st.data.ani = ani;
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::updateAnimation(ani);
}

void UndoSpriteState::updateAframe(const LvkAframe& aframe, Id aniId)
{
    if (aframe == _animations[aniId].aframes[aframe.id]) {
        return;
    }

    StateChange st;
    st.type = StateCircularBuffer::st_updateAframe;
    st.data.ani.id = aniId;
    st.data.old_aframe = _animations[aniId].aframes[aframe.id];
    st.data.aframe = aframe;
    _stBuffer.addState(st);
    _unsaved = true;

    SpriteState::updateAframe(aframe, aniId);
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
