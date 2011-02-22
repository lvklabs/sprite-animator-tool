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
    if (!canUndo()) {
        return false;
    }

#ifdef DEBUG_UNDO
    qDebug() << "--- Undo ---";
#endif

    StateChange st = _stBuffer.currentState();
    _stBuffer.prevState();

    if (st.type == StateCircularBuffer::st_transactionEnd) {
        do {
            st = _stBuffer.currentState();
            _stBuffer.prevState();
            undo(st);
        } while (st.type != StateCircularBuffer::st_transactionStart);
    } else {
        undo(st);
    }

    return true;
}

bool SpriteState2::undo(StateChange &st)
{
    switch (st.type) {

    /* undo update */
    case StateCircularBuffer::st_updateImage:
        st.data.old_img.reloadImage();
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
        st.data.img.reloadImage();
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


bool SpriteState2::redo()
{
    if (!canRedo()) {
        return false;
    }

#ifdef DEBUG_UNDO
    qDebug() << "--- Redo --- ";
#endif

    _stBuffer.nextState();
    StateChange st = _stBuffer.currentState();

    if (st.type == StateCircularBuffer::st_transactionStart) {
        do {
            _stBuffer.nextState();
            st = _stBuffer.currentState();
            redo(st);
        } while (st.type != StateCircularBuffer::st_transactionEnd);
    } else {
        redo(st);
    }

    return true;
}

bool SpriteState2::redo(StateChange &st)
{
    switch (st.type) {

    /* redo update */
    case StateCircularBuffer::st_updateImage:
        st.data.img.reloadImage();
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
        st.data.img.reloadImage();
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

bool SpriteState2::canUndo() const
{
    return _stBuffer.hasPrevState();
}

bool SpriteState2::canRedo() const
{
    return _stBuffer.hasNextState();
}

bool SpriteState2::hasUnsavedChanges() const
{
    return !_stBuffer.hasSavedFlag();
}

void SpriteState2::startTransaction()
{
    StateChange st;
    st.type = StateCircularBuffer::st_transactionStart;
    _stBuffer.addState(st);
}

void SpriteState2::endTransaction()
{
    StateChange st;
    st.type = StateCircularBuffer::st_transactionEnd;
    _stBuffer.addState(st);
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
    st.data.old_img.freeImageData();
    st.data.img = img;
    st.data.img.freeImageData();
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
    if (aframe == _animations[aniId].aframe(aframe.id)) {
        return;
    }

    StateChange st;
    st.type = StateCircularBuffer::st_updateAframe;
    st.data.ani.id = aniId;
    st.data.old_aframe = _animations[aniId].aframe(aframe.id);
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
    st.data.img.freeImageData();
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
    st.data.img.freeImageData();
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
    st.data.aframe = _animations[aniId].aframe(aframeId);
    st.data.ani.id = aniId;
    _stBuffer.addState(st);

    SpriteState::removeAframe(aframeId, aniId);
}
