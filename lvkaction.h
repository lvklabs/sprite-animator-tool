#ifndef LVKACTION_H
#define LVKACTION_H

#include <QAction>

/// This class is identical to QAction excepts that emits a new signal
/// triggered(const QString& filename)
/// This class is used to open recent files through the "Open recent" menu.
class LvkAction : public QAction
{
    Q_OBJECT

public:
    LvkAction(const QString& text = "", QObject* parent = 0);

signals:
    void triggered(const QString& text);

private slots:
    void triggerText();
};

#endif // LVKACTION_H
