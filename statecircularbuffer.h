#ifndef STATECIRCULARBUFFER_H
#define STATECIRCULARBUFFER_H

#include <QString>

#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"
#include "settings.h"

/// This is an implementation of a circular buffer that stores sprite state changes.
/// This is a helper class used in StateSprite2 to implement undo an redo.
/// It also allows to mark an state as saved. After undoing and redoing, it keeps
/// the track if we are in the saved or not.
class StateCircularBuffer
{
public:

    enum StateChangeType {
        st_null,
        st_updateImage,
        st_updateFrame,
        st_updateAnimation,
        st_updateAframe,
        st_addImage,
        st_addFrame,
        st_addAnimation,
        st_addAframe,
        st_removeImage,
        st_removeFrame,
        st_removeAnimation,
        st_removeAframe,
        st_reloadImages,
        st_reloadFramePixmap,
        st_reloadFramePixmaps,
    };

    struct Data {
        InputImage   old_img;
        LvkFrame     old_frame;
        LvkAnimation old_ani;
        LvkAframe    old_aframe;

        InputImage   img;
        LvkFrame     frame;
        LvkAnimation ani;
        LvkAframe    aframe;

        bool operator==(const Data& d)
        {
            return old_img    == d.old_img    &&
                   old_frame  == d.old_frame  &&
                   old_ani    == d.old_ani    &&
                   old_aframe == d.old_aframe &&
                   img        == d.img        &&
                   frame      == d.frame      &&
                   ani        == d.ani        &&
                   aframe     == d.aframe;
        }
    };

    struct StateChange {
        StateChange() : type(st_null) { }

        StateChangeType type;
        Data            data;

        bool operator==(const StateChange& st)
        { return type == st.type && data == st.data; }
    };

    static const int BUFF_SIZE = MAX_UNDO_TIMES;

    StateCircularBuffer();
    ~StateCircularBuffer();

    void addState(const StateChange& st);
    StateChange currentState();
    bool hasNextState();
    bool hasPrevState();
    void nextState();
    void prevState();
    void clear();

    /// mark current state as saved (only the current state will have the saved flag)
    void setSavedFlag();

    /// returns true if the current state has the saved flag
    bool hasSavedFlag();

    QString toString();

private:

    StateChange *_buf;    /* buffer */
    int          _i;      /* index to implement a circular buffer */
    int          _first;  /* first position in the buffer */
    int          _saved;  /* saved position in the buffer, -1 if not saved */

    inline int inc(int& i)
    {
        i = (i + 1) % BUFF_SIZE;
        if (i < 0) {
            i += BUFF_SIZE;
        }
        return i;
    }

    inline int dec(int& i)
    {
        i = (i - 1) % BUFF_SIZE;
        if (i < 0) {
            i += BUFF_SIZE;
        }
        return i;
    }
};

typedef StateCircularBuffer::StateChange StateChange;

#endif // STATECIRCULARBUFFER_H
