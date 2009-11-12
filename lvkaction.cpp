#include "lvkaction.h"

LvkAction::LvkAction(const QString& text, QObject* parent)
        : QAction(text, parent)
{
    connect(this, SIGNAL(triggered()), this, SLOT(triggerText()));
}

void LvkAction::triggerText()
{
    emit triggered(text());
}
