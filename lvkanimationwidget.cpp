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

void LvkAnimationWidget::setAnimation(const LvkAnimation& ani, const QMap<Id, QPixmap>& fpixmaps)
{
    clear();

    for (QListIterator<LvkAframe> it(ani._aframes); it.hasNext();) {
        LvkAframe aframe = it.next();
        _fpixmaps << QPixmap(fpixmaps.value(aframe.frameId));
        _delays   << aframe.delay;
        _oxs      << aframe.ox;
        _oys      << aframe.oy;
        _stickies << aframe.sticky;
    }

    repaint();
}

void LvkAnimationWidget::setAnimations(const QList<LvkAnimation>& anis, const QMap<Id, QPixmap>& fpixmaps)
{
    clear();

    for (QListIterator<LvkAnimation> aniIter(anis); aniIter.hasNext();) {
        for (QListIterator<LvkAframe> aframeIter(aniIter.next()._aframes); aframeIter.hasNext();) {
            LvkAframe aframe = aframeIter.next();
            _fpixmaps << QPixmap(fpixmaps.value(aframe.frameId));
            _delays   << aframe.delay;
            _oxs      << aframe.ox;
            _oys      << aframe.oy;
            _stickies << aframe.sticky;
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
    _stickies.clear();
    _origin.setX(1);
    _origin.setY(1);

    repaint();
}

void LvkAnimationWidget::nextFrame()
{
    // Search for the first aframe without sticky flag
    do {
        _currentFrame++;
    } while (_currentFrame < _stickies.size() && _stickies[_currentFrame]);

    if (_currentFrame >= _fpixmaps.size()) {
        _currentFrame = 0;

        while (_currentFrame < _stickies.size() && _stickies[_currentFrame]) {
            _currentFrame++;
        }

        // If this happens if because we only have aframes with sticky flag
        if(_currentFrame >= _fpixmaps.size()) {
            _currentFrame = 0;
        }
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

    // Draw current frame and frames with the sticky flag
    for (int i = 0; i < _fpixmaps.size(); ++i) {
        if (i == _currentFrame || _stickies[i]) {
            painter.drawPixmap(_origin.x() + _oxs[i], _origin.y() + _oys[i], _fpixmaps[i]);
        }
    }

    // Draw screen rect
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
