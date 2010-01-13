#include "lvkanimationwidget.h"

#include <QDebug>

LvkAnimationWidget::LvkAnimationWidget(const LvkAnimation& ani, const QHash<Id, QPixmap>& fpixmaps, QObject* parent)
        : QObject(parent), currentFrame(-1), currentTimer(0), animated(false)
{
    for (QHashIterator<Id, LvkAframe> it(ani.aframes); it.hasNext();) {
        LvkAframe aframe = it.next().value();
        pixmaps << new QGraphicsPixmapItem(fpixmaps.value(aframe.frameId));
        delays  << aframe.delay;
    }

   for (int i = 0; i < pixmaps.size(); ++i) {
        pixmaps[i]->setVisible(false);
        addToGroup(pixmaps[i]);
    }
}

LvkAnimationWidget::~LvkAnimationWidget()
{
    while (!pixmaps.isEmpty()) {
        delete pixmaps.takeFirst();
    }
}

int LvkAnimationWidget::nextFrame()
{
    currentFrame++;

    if(currentFrame >= pixmaps.size()) {
        currentFrame = 0;
    }
    return currentFrame;
}

void LvkAnimationWidget::timerEvent(QTimerEvent* /*evt*/)
{
    killTimer(currentTimer);
    pixmaps[currentFrame]->setVisible(false);
    nextFrame();
    pixmaps[currentFrame]->setVisible(true);
    currentTimer = startTimer(delays[currentFrame]);
}

void LvkAnimationWidget::startAnimation()
{
    if (pixmaps.size() > 0) {
        nextFrame();
        currentTimer = startTimer(delays[currentFrame]);
        pixmaps[currentFrame]->setVisible(true);
        animated = true;
    }
}

void LvkAnimationWidget::stopAnimation()
{
    killTimer(currentTimer);
    currentFrame = -1;
    currentTimer = 0;
    animated = false;
}

bool LvkAnimationWidget::isAnimated()
{
    return(animated);
}
