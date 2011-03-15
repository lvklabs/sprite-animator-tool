#include "lvkanimationwidget.h"

#include <QDebug>
#include <QPainter>
#include <QMouseEvent>


LvkAnimationWidget::LvkAnimationWidget(QWidget* parent)
        : QWidget(parent), _currentFrame(-1), _currentTimer(0), _isPlaying(false), _scrW(320), _scrH(480),
          _origin(QPoint(1,1))
{
    setCursor(QCursor(Qt::OpenHandCursor));
}

void LvkAnimationWidget::setAnimation(const LvkAnimation& ani, const QHash<Id, QPixmap>& fpixmaps)
{
    clear();

    for (QListIterator<LvkAframe> it(ani._aframes); it.hasNext();) {
        LvkAframe aframe = it.next();
        _fpixmaps << QPixmap(fpixmaps.value(aframe.frameId));
        _delays   << aframe.delay;
        _oxs      << aframe.ox;
        _oys      << aframe.oy;
    }

    repaint();
}

void LvkAnimationWidget::setAnimations(const QList<LvkAnimation>& anis, const QHash<Id, QPixmap>& fpixmaps)
{
    clear();

    for (QListIterator<LvkAnimation> aniIter(anis); aniIter.hasNext();) {
        for (QListIterator<LvkAframe> aframeIter(aniIter.next()._aframes); aframeIter.hasNext();) {
            LvkAframe aframe = aframeIter.next();
            _fpixmaps << QPixmap(fpixmaps.value(aframe.frameId));
            _delays   << aframe.delay;
            _oxs      << aframe.ox;
            _oys      << aframe.oy;
        }
    }

    repaint();
}

void LvkAnimationWidget::setScreenSize(int w, int h)
{
    if (w > 0 && h > 0) {
        _scrW = w;
        _scrH = h;
        setGeometry(x(), y(), w + 5, h + 5);
    }

    repaint();
}

void LvkAnimationWidget::clear()
{
    stop();

    _fpixmaps.clear();
    _delays.clear();
    _oxs.clear();
    _oys.clear();
    _origin.setX(1);
    _origin.setY(1);

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
        painter.drawPixmap(_origin.x() + _oxs[_currentFrame],
                           _origin.y() + _oys[_currentFrame],
                           _fpixmaps[_currentFrame]);
    }
    painter.drawRect(0, 0, _scrW, _scrH);
}

void LvkAnimationWidget::mousePressEvent(QMouseEvent */*event*/)
{
    setCursor(QCursor(Qt::ClosedHandCursor));
}

void LvkAnimationWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() == Qt::LeftButton && _currentFrame != -1) {
        _origin.setX(event->x() - _fpixmaps[_currentFrame].width()/2);
        _origin.setY(event->y() - _fpixmaps[_currentFrame].height()/2);
    } else {
        _origin.setX(1);
        _origin.setY(1);
    }

    repaint();
}

void LvkAnimationWidget::mouseReleaseEvent(QMouseEvent */*event*/)
{
    setCursor(QCursor(Qt::OpenHandCursor));
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
