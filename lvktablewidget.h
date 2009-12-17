#ifndef LVKTABLEWIDGET_H
#define LVKTABLEWIDGET_H

#include <QTableWidget>
#include <QKeyEvent>

class LvkTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    LvkTableWidget(QWidget *parent = 0);

protected:
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
};

#endif // LVKTABLEWIDGET_H
