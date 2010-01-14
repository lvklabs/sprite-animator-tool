#ifndef SPRITESTATE2_H
#define SPRITESTATE2_H

#include "spritestate.h"
#include "statecircularbuffer.h"

class QObject;


/// this class extends SpriteState to provide undo, redo features.
/// it also provides the method hasUnsavedChanges()
class SpriteState2 : public SpriteState
{
public:
    SpriteState2(QObject* parent = 0);

    bool undo();
    bool redo();

    bool canUndo();
    bool canRedo();

    bool hasUnsavedChanges();

    /* inherited methods */

    void updateImage(const InputImage& img);
    void updateFrame(const LvkFrame& frame);
    void updateAnimation(const LvkAnimation& ani);
    void updateAframe(const LvkAframe& aframe, Id aniId);

    void addImage(InputImage& img);
    void addFrame(LvkFrame& frame);
    void addAnimation(LvkAnimation& ani);
    void addAframe(LvkAframe& aframe, Id aniId);

    void removeImage(Id id);
    void removeFrame(Id id);
    void removeAnimation(Id id);
    void removeAframe(Id aframeId, Id aniId);

    void clear();
    bool save(const QString& filename, SpriteStateError* err = 0);
    bool load(const QString& filename, SpriteStateError* err = 0);

private:
    StateCircularBuffer _stBuffer;
};

#endif // SPRITESTATE2_H
