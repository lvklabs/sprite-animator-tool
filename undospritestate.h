#ifndef UNDOSPRITESTATE_H
#define UNDOSPRITESTATE_H

#include "spritestate.h"
#include "statecircularbuffer.h"

class QObject;


/// this class extends SpriteState to provide undo, redo features.
/// it also provides the method hasUnsavedChanges()
class UndoSpriteState : public SpriteState
{
public:
    UndoSpriteState(QObject* parent = 0);

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

    void addImage(const InputImage& img);
    void addFrame(const LvkFrame& frame);
    void addAnimation(const LvkAnimation& ani);
    void addAframe(const LvkAframe& aframe, Id aniId);

    void removeImage(Id id);
    void removeFrame(Id id);
    void removeAnimation(Id id);
    void removeAframe(Id aframeId, Id aniId);

    void clear();
    bool save(const QString& filename, SpriteStateError* err = 0);
    bool load(const QString& filename, SpriteStateError* err = 0);

private:
    StateCircularBuffer _stBuffer;

    bool _unsaved;

};

#endif // UNDOSPRITESTATE_H
