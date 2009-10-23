#include "lvkframegraphicsgroup.h"


LvkFrameGraphicsGroup::LvkFrameGraphicsGroup(QObject* parent)
{

}

LvkFrameGraphicsGroup::LvkFrameGraphicsGroup(QList<QGraphicsPixmapItem*> frames, double del[]):children(frames),
delays(del),currentFrame(-1),currentTimer(0)
{
     //TODO: if (frames==null)

    for(int i=0; i<children.size(); i++)
    {
        children[i]->setVisible(false);
        this->addToGroup(children[i]);
    }
}

int LvkFrameGraphicsGroup::nextFrame()
{

    if(currentFrame >= children.size())
    {
        currentFrame=-1;
    }
    currentFrame++;
    return currentFrame;
}

void LvkFrameGraphicsGroup::timerEvent(QTimerEvent* evt)
{
    this->killTimer(currentTimer);
    children[currentFrame]->setVisible(false);
    children[this->nextFrame()]->setVisible(true);
    currentTimer = this->startTimer(delays[currentFrame]*500);
}

void LvkFrameGraphicsGroup::startAnimation()
{
    currentTimer = this->startTimer(delays[this->nextFrame()]*500);
    children[currentFrame]->setVisible(true);
}

void LvkFrameGraphicsGroup::run()
{
    exec();
    startAnimation();

    while(true){}
}

void LvkFrameGraphicsGroup::stopAnimation()
{
    this->killTimer(currentTimer);
    currentFrame=-1;
    currentTimer=0;
}
