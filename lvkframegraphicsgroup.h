#ifndef LVKFRAMEGRAPHICSGROUP_H
#define LVKFRAMEGRAPHICSGROUP_H

#include <QObject>
#include <QGraphicsItemGroup>
#include <QList>
#include <QGraphicsPixmapItem>
#include <QTimerEvent>
#include <QObject>

#include "lvkanimation.h"


/// Groups all frames belonging to a single animation in a single QGraphicsItem,
/// and implements the logic that will animate those frames.
class LvkFrameGraphicsGroup : public QGraphicsItemGroup, public QObject
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
    LvkFrameGraphicsGroup(const LvkAnimation& ani, const QHash<Id, QPixmap>& fpixmaps, QObject* parent = 0);
    ~LvkFrameGraphicsGroup();

    void startAnimation();
    void stopAnimation();

private:
    /// Returns the location of the next frame of the animation
    int nextFrame();

    /// QObjects override that handles each timeout event
    /// kills current timer, sets the current frame as invisible,
    /// sets nextFrames timer with its corresponding delay and makes
    /// next frame visible
    void timerEvent(QTimerEvent* evt);

    /// hide copy and assign constructor
    LvkFrameGraphicsGroup(const LvkFrameGraphicsGroup&);
    LvkFrameGraphicsGroup& operator=(const LvkFrameGraphicsGroup&);
};

#endif // LVKFRAMEGRAPHICSGROUP_H
