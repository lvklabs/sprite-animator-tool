#include "lvkanimationwidget.h"

#include <QDebug>
#include <QPainter>


LvkAnimationWidget::LvkAnimationWidget(QWidget* parent, const LvkAnimation& ani, const QHash<Id, QPixmap>& fpixmaps)
        : QWidget(parent), _currentFrame(-1), _currentTimer(0), _isPlaying(false), _scrW(320), _scrH(480)
{
    setAnimation(ani, fpixmaps);
}

void LvkAnimationWidget::setAnimation(const LvkAnimation& ani, const QHash<Id, QPixmap>& fpixmaps)
{
    clear();

    for (QHashIterator<Id, LvkAframe> it(ani.aframes); it.hasNext();) {
        LvkAframe aframe = it.next().value();
        _fpixmaps << QPixmap(fpixmaps.value(aframe.frameId));
        _delays  << aframe.delay;
    }

    repaint();
}

void LvkAnimationWidget::setScreenSize(int w, int h)
{
    if (w > 0 && h > 0) {
        _scrW = w;
        _scrH = h;
    }

    repaint();
}

void LvkAnimationWidget::clear()
{
    stop();
    _fpixmaps.clear();
    _delays.clear();

    repaint();
}

void LvkAnimationWidget::nextFrame()
{
    _currentFrame++;

    if(_currentFrame >= _fpixmaps.size()) {
        _currentFrame = 0;
    }

    repaint();
}

void LvkAnimationWidget::timerEvent(QTimerEvent* /*event*/)
{
    killTimer(_currentTimer);
    nextFrame();
    _currentTimer = startTimer(_delays[_currentFrame]);
}

void LvkAnimationWidget::paintEvent(QPaintEvent */*event*/)
{
    QPainter painter(this);
    if (_fpixmaps.size() > 0 && _currentFrame >= 0) {
        painter.drawPixmap(1, 1, _fpixmaps[_currentFrame]);
    }
    painter.drawRect(0, 0, _scrW - 1, _scrH - 1);
}

void LvkAnimationWidget::play()
{
    if (_fpixmaps.size() > 0) {
        nextFrame();
        _currentTimer = startTimer(_delays[_currentFrame]);
        _isPlaying = true;
    }

    repaint();
}

void LvkAnimationWidget::stop()
{
    killTimer(_currentTimer);
    _currentFrame = -1;
    _currentTimer = 0;
    _isPlaying = false;

    repaint();
}

bool LvkAnimationWidget::isPlaying()
{
    return _isPlaying;
}
