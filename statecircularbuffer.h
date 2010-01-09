#ifndef STATECIRCULARBUFFER_H
#define STATECIRCULARBUFFER_H

#include "inputimage.h"
#include "lvkframe.h"
#include "lvkanimation.h"
#include "lvkaframe.h"
#include "settings.h"

class StateCircularBuffer
{
public:

    enum StateChangeType {
        st_null,
        st_mutableGetImage,
        st_mutableGetFrame,
        st_mutableGetAnimation,
        st_mutableGetAframe,
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
    };

    struct StateChange {
        StateChange() : type(st_null) { }

        StateChangeType type;
        Data            data;
    };

    static const int BUFF_SIZE = MAX_UNDO_TIMES;

    StateCircularBuffer();
    ~StateCircularBuffer();

    void addState(StateChange st);
    StateChange currentState();
    bool hasNextState();
    bool hasPrevState();
    void nextState();
    void prevState();
    void clear();

private:

    StateChange *_buf;    /* buffer */
    int          _i;      /* index to implement a circular buffer */
    int          _first;  /* first position in the buffer */

    inline int inc(int& i)
    {
        i++;
        if (i < 0) {
            return i % BUFF_SIZE + BUFF_SIZE; /* i % BUFFER_SIZE is negative */
        } else {
            return i % BUFF_SIZE;
        }
    }

    inline int dec(int& i)
    {
        i--;
        if (i < 0) {
            return i % BUFF_SIZE + BUFF_SIZE; /* i % BUFFER_SIZE is negative */
        } else {
            return i % BUFF_SIZE;
        }
    }
};



#endif // STATECIRCULARBUFFER_H
