#ifndef LVKFRAMEGRAPHICSGROUP_H
#define LVKFRAMEGRAPHICSGROUP_H

#include <QObject>
#include <QGraphicsItemGroup>
#include <QList>
#include <QGraphicsPixmapItem>
#include <QTimerEvent>
#include <QThread>


/*
 * Groups all frames belonging to a single animation in a single QGraphicsItem,
 * and implements the logic that will animate those frames.
 *
 */
/// TODO
/// It is currently not working correctly, for this reason it inherits from QThread so
/// that each instance will run as a different thread with a separate event loop.
///
class LvkFrameGraphicsGroup : public QThread, public QGraphicsItemGroup
{
private:
    /// groups all of the animations frames
    QList<QGraphicsPixmapItem*> children;
    /// container with a delay for each frame
    double* delays;
    /// current visible frame in the animation
    int currentFrame;
    /// running timer ID
    int currentTimer;
public:

    Q_INVOKABLE LvkFrameGraphicsGroup(QObject* parent=0);
    /// mediante una copia de la lista se asegura el lifetime de sus
    /// objetos
    LvkFrameGraphicsGroup(QList<QGraphicsPixmapItem*> frames, double del[]);

    void startAnimation();
    void stopAnimation();

    /// QThread method that initiates execution of threard
    void run();

private:

    /// Returns the location of the next frame of the animation
    int nextFrame();

    /// QObjects override that handles each timeout event
    /// kills current timer, sets the current frame as invisible,
    /// sets nextFrames timer with its corresponding delay and makes
    /// next frame visible
    void timerEvent(QTimerEvent* evt);
};

#endif // LVKFRAMEGRAPHICSGROUP_H
