#ifndef UNDOSPRITESTATE_H
#define UNDOSPRITESTATE_H

#include "spritestate.h"

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

private:

};

#endif // UNDOSPRITESTATE_H
