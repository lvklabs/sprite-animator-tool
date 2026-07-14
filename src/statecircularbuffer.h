#ifndef STATECIRCULARBUFFER_H
#define STATECIRCULARBUFFER_H

#include <QString>

#include <memory>

#include "inputimage.h"
#include "lvkaframe.h"
#include "lvkanimation.h"
#include "lvkframe.h"
#include "settings.h"

/// This is an implementation of a circular buffer that stores changes in the sprite state.
/// This is a helper class used in StateSprite2 to implement undo and redo.
/// It also allows to mark an state as saved.
class StateCircularBuffer {
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
        st_transactionStart,
        st_transactionEnd,
    };

    struct Data {
        InputImage old_img;
        LvkFrame old_frame;
        LvkAnimation old_ani;
        LvkAframe old_aframe;

        InputImage img;
        LvkFrame frame;
        LvkAnimation ani;
        LvkAframe aframe;

        /// list position of `aframe` inside its animation at the time a
        /// st_removeAframe change was recorded; undo re-inserts at this
        /// position so playback order survives a remove+undo. -1 = unset
        /// (append).
        int aframeIndex = -1;

        bool operator==(const Data &d) {
            return old_img == d.old_img && old_frame == d.old_frame && old_ani == d.old_ani &&
                   old_aframe == d.old_aframe && img == d.img && frame == d.frame && ani == d.ani &&
                   aframe == d.aframe && aframeIndex == d.aframeIndex;
        }
    };

    struct StateChange {
        StateChange() : type(st_null) {}

        StateChangeType type;
        Data data;

        bool operator==(const StateChange &st) { return type == st.type && data == st.data; }
    };

    static const int BUFF_SIZE = MAX_UNDO_TIMES;

    StateCircularBuffer();
    ~StateCircularBuffer();

    void addState(const StateChange &st);
    StateChange currentState() const;
    bool hasNextState() const;
    bool hasPrevState() const;
    void nextState();
    void prevState();
    void clear();

    /// mark current state as saved (only the current state will have the saved flag)
    void setSavedFlag();

    /// returns true if the current state has the saved flag
    bool hasSavedFlag() const;

    /// returns the string representation
    QString toString() const;

private:
    // Agent 7: was `StateChange *_buf;` with manual new[]/delete[].
    // Switched to std::unique_ptr<StateChange[]> so the BUFF_SIZE-element
    // ring buffer is freed automatically and we lose the manual dtor.
    std::unique_ptr<StateChange[]> _buf;
    int _i;     /* index to implement a circular buffer */
    int _first; /* first position in the buffer */
    int _saved; /* saved position in the buffer, -1 if not saved */

    inline int inc(int &i) const {
        i = (i + 1) % BUFF_SIZE;
        if (i < 0) {
            i += BUFF_SIZE;
        }
        return i;
    }

    inline int dec(int &i) const {
        i = (i - 1) % BUFF_SIZE;
        if (i < 0) {
            i += BUFF_SIZE;
        }
        return i;
    }
};

typedef StateCircularBuffer::StateChange StateChange;

#endif // STATECIRCULARBUFFER_H
