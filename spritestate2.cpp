#include <QObject>
#include <QDebug>

#include "spritestate.h"
#include "spritestate2.h"

SpriteState2::SpriteState2(QObject* parent)
    : SpriteState(parent)
{
}

bool SpriteState2::undo()
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
            break;

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

        return true;
    }
    return false;
}

bool SpriteState2::redo()
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
            break;

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

        return true;
    }
    return false;
}

bool SpriteState2::canUndo()
{
    return _stBuffer.hasPrevState();
}

bool SpriteState2::canRedo()
{
    return _stBuffer.hasNextState();
}

bool SpriteState2::hasUnsavedChanges()
{
    return !_stBuffer.hasSavedFlag();
}

 /* inherited methods */

// Load, save, export ******************************************************

bool SpriteState2::save(const QString& filename, SpriteStateError* err)
{
    bool success = SpriteState::save(filename, err);

    if (success) {
        _stBuffer.setSavedFlag();
    }

    return success;
}

bool SpriteState2::load(const QString& filename, SpriteStateError* err)
{
    bool success = SpriteState::load(filename, err);

    if (success) {
        _stBuffer.clear();
    }

    return success;
}

void SpriteState2::clear()
{
    _stBuffer.clear();
    SpriteState::clear();
}

// update *******************************************************************

void SpriteState2::updateImage(const InputImage& img)
{
    if (img == _images[img.id]) {
        return;
    }

    StateChange st;
    st.type = StateCircularBuffer::st_updateImage;
    st.data.old_img = _images[img.id];
    st.data.img = img;
    _stBuffer.addState(st);

    SpriteState::updateImage(img);
}

void SpriteState2::updateFrame(const LvkFrame& frame)
{
    if (frame == _frames[frame.id]) {
        return;
    }

    StateChange st;
    st.type = StateCircularBuffer::st_updateFrame;
    st.data.old_frame = _frames[frame.id];
    st.data.frame = frame;
    _stBuffer.addState(st);

    SpriteState::updateFrame(frame);
}

void SpriteState2::updateAnimation(const LvkAnimation& ani)
{
    if (ani == _animations[ani.id]) {
        return;
    }

    StateChange st;
    st.type = StateCircularBuffer::st_updateAnimation;
    st.data.old_ani = _animations[ani.id];
    st.data.ani = ani;
    _stBuffer.addState(st);

    SpriteState::updateAnimation(ani);
}

void SpriteState2::updateAframe(const LvkAframe& aframe, Id aniId)
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

    SpriteState::updateAframe(aframe, aniId);
}

// add *********************************************************************

void SpriteState2::addImage(InputImage& img)
{
    SpriteState::addImage(img);

    StateChange st;
    st.type = StateCircularBuffer::st_addImage;
    st.data.img = img;
    _stBuffer.addState(st);
}

void SpriteState2::addFrame(LvkFrame& frame)
{
    SpriteState::addFrame(frame);

    StateChange st;
    st.type = StateCircularBuffer::st_addFrame;
    st.data.frame = frame;
    _stBuffer.addState(st);
}

void SpriteState2::addAnimation(LvkAnimation& ani)
{
    SpriteState::addAnimation(ani);

    StateChange st;
    st.type = StateCircularBuffer::st_addAnimation;
    st.data.ani = ani;
    _stBuffer.addState(st);
}

void SpriteState2::addAframe(LvkAframe& aframe, Id aniId)
{
    SpriteState::addAframe(aframe, aniId);

    StateChange st;
    st.type = StateCircularBuffer::st_addAframe;
    st.data.aframe = aframe;
    st.data.ani.id = aniId;
    _stBuffer.addState(st);
}

// remove ******************************************************************

void SpriteState2::removeImage(Id id)
{
    StateChange st;
    st.type = StateCircularBuffer::st_removeImage;
    st.data.img = _images[id];
    _stBuffer.addState(st);

    SpriteState::removeImage(id);
}

void SpriteState2::removeFrame(Id id)
{
    StateChange st;
    st.type = StateCircularBuffer::st_removeFrame;
    st.data.frame = _frames[id];
    _stBuffer.addState(st);

    SpriteState::removeFrame(id);
}

void SpriteState2::removeAnimation(Id id)
{
    StateChange st;
    st.type = StateCircularBuffer::st_removeAnimation;
    st.data.ani = _animations[id];
    _stBuffer.addState(st);

    SpriteState::removeAnimation(id);
}

void SpriteState2::removeAframe(Id aframeId, Id aniId)
{
    StateChange st;
    st.type = StateCircularBuffer::st_removeAframe;
    st.data.aframe = _animations[aniId].aframes[aframeId];
    st.data.ani.id = aniId;
    _stBuffer.addState(st);

    SpriteState::removeAframe(aframeId, aniId);
}
