#include "qrecentfileaction.h"

QRecentFileAction::QRecentFileAction(const QString& filename, QObject* parent)
        : QAction(filename, parent)
{
    connect(this, SIGNAL(triggered()), this, SLOT(triggerFilename()));
}

void QRecentFileAction::triggerFilename()
{
    emit triggered(text());
}
