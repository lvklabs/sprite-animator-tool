#include "lvkframegraphicsgroup.h"

#include <QDebug>

LvkFrameGraphicsGroup::LvkFrameGraphicsGroup(const LvkAnimation& ani, const QHash<Id, QPixmap>& fpixmaps, QObject* parent)
        : QObject(parent), delays(new double[ani.aframes.size()]), currentFrame(-1), currentTimer(0)
{
    int i = 0;

    for (QHashIterator<Id, LvkAframe> it(ani.aframes); it.hasNext();) {
        QGraphicsPixmapItem* aux = new QGraphicsPixmapItem(fpixmaps.value(it.next().key()));
        children << aux;
        delays[i++] = it.value().delay;
    }

   for (i = 0; i < children.size(); ++i) {
        children[i]->setVisible(false);
        addToGroup(children[i]);
    }
}

LvkFrameGraphicsGroup::~LvkFrameGraphicsGroup()
{
    delete delays;
}

int LvkFrameGraphicsGroup::nextFrame()
{
    currentFrame++;

    if(currentFrame >= children.size()) {
        currentFrame = 0;
    }
    return currentFrame;
}

void LvkFrameGraphicsGroup::timerEvent(QTimerEvent* /*evt*/)
{
    killTimer(currentTimer);
    children[currentFrame]->setVisible(false);
    nextFrame();
    children[currentFrame]->setVisible(true);
    currentTimer = startTimer(delays[currentFrame]);
}

void LvkFrameGraphicsGroup::startAnimation()
{
    if (children.size() > 0) {
        nextFrame();
        currentTimer = startTimer(delays[currentFrame]);
        children[currentFrame]->setVisible(true);
    }
}

void LvkFrameGraphicsGroup::stopAnimation()
{
    killTimer(currentTimer);
    currentFrame = -1;
    currentTimer = 0;
}
