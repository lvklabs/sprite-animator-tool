#ifndef QRECENTFILEACTION_H
#define QRECENTFILEACTION_H

#include <QAction>

/// This class is used to open recent files through the "Open recent" menu.
/// It is identical to QAction excepts that emits a new signal
/// triggered(const QString& filename) in order to know which file we must open
class QRecentFileAction : public QAction
{
    Q_OBJECT

public:
    QRecentFileAction(const QString& filename = "", QObject* parent = 0);

signals:
    void triggered(const QString& filename);

private slots:
    void triggerFilename();
};

#endif // QRECENTFILEACTION_H
