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
class LvkAnimationWidget : public QGraphicsItemGroup, public QObject
{
private:
    /// container with the pixmap data for each frame
    QList<QGraphicsPixmapItem*> pixmaps;
    /// container with a delay for each frame
    QList<int> delays;
    /// current visible frame in the animation
    int currentFrame;
    /// running timer ID
    int currentTimer;
    /// flag for Animation information
    bool animated;

public:
    LvkAnimationWidget(const LvkAnimation& ani, const QHash<Id, QPixmap>& fpixmaps, QObject* parent = 0);
    ~LvkAnimationWidget();

    void startAnimation();
    void stopAnimation();
    bool isAnimated();

private:
    /// Returns the location of the next frame of the animation
    int nextFrame();

    /// QObjects override that handles each timeout event
    /// kills current timer, sets the current frame as invisible,
    /// sets nextFrames timer with its corresponding delay and makes
    /// next frame visible
    void timerEvent(QTimerEvent* evt);

    /// hide copy and assign constructor
    LvkAnimationWidget(const LvkAnimationWidget&);
    LvkAnimationWidget& operator=(const LvkAnimationWidget&);
};

#endif // LVKFRAMEGRAPHICSGROUP_H
