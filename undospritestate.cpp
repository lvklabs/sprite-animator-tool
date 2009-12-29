#include <QObject>

#include "spritestate.h"
#include "undospritestate.h"

UndoSpriteState::UndoSpriteState(QObject* parent)
    : SpriteState(parent)
{
}


bool UndoSpriteState::undo()
{
    // TODO
    return false;
}

bool UndoSpriteState::redo()
{
    // TODO
    return false;
}

bool UndoSpriteState::canUndo()
{
    // TODO
    return false;
}

bool UndoSpriteState::canRedo()
{
    // TODO
    return false;
}

bool UndoSpriteState::hasUnsavedChanges()
{
    // TODO
    return true;
}
