#include "lvkaction.h"

LvkAction::LvkAction(const QString &text, QObject *parent) : QAction(text, parent) {
    // QAction::triggered(bool) is the only overload in QAction; LvkAction adds
    // its own triggered(const QString&) signal so the unqualified PMF would be
    // ambiguous. Bind explicitly to the inherited (bool) overload.
    connect(this, &QAction::triggered, this, [this](bool) { triggerText(); });
}

void LvkAction::triggerText() {
    emit triggered(text());
}
