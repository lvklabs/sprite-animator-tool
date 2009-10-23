#ifndef LVKFRAMEGRAPHICSGROUP_H
#define LVKFRAMEGRAPHICSGROUP_H

#include <QObject>
#include <QGraphicsItemGroup>
#include <QList>
#include <QGraphicsPixmapItem>
#include <QTimerEvent>
#include <QThread>

class LvkFrameGraphicsGroup : public QThread, public QGraphicsItemGroup
{



private:
    QList<QGraphicsPixmapItem*> children;
    double* delays;
    int currentFrame;
    int currentTimer;
public:
    /// mediante una copia de la lista se asegura el lifetime de sus
    /// objetos
    Q_INVOKABLE LvkFrameGraphicsGroup(QObject* parent=0);
    LvkFrameGraphicsGroup(QList<QGraphicsPixmapItem*> frames, double del[]);
    void startAnimation();
    void stopAnimation();
    void run();

private:

    int nextFrame();
    void timerEvent(QTimerEvent* evt);
};

#endif // LVKFRAMEGRAPHICSGROUP_H
