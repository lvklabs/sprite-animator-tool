#include "lvkanimationwidget.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>

LvkAnimationWidget::LvkAnimationWidget(QWidget *parent)
    : QWidget(parent), _currentFrame(-1), _currentTimer(0), _isPlaying(false), _scrW(320),
      _scrH(480), _origin(QPoint(1, 1)) {
    setCursor(QCursor(Qt::OpenHandCursor));
}

void LvkAnimationWidget::setAnimation(const LvkAnimation &ani, const QMap<Id, QPixmap> &fpixmaps) {
    clear();

    for (QListIterator<LvkAframe> it(ani._aframes); it.hasNext();) {
        LvkAframe aframe = it.next();
        _fpixmaps << QPixmap(fpixmaps.value(aframe.frameId));
        _delays << aframe.delay;
        _oxs << aframe.ox;
        _oys << aframe.oy;
        _stickies << aframe.sticky;
    }

    repaint();
}

void LvkAnimationWidget::setAnimations(const QList<LvkAnimation> &anis,
                                       const QMap<Id, QPixmap> &fpixmaps) {
    clear();

    for (QListIterator<LvkAnimation> aniIter(anis); aniIter.hasNext();) {
        for (QListIterator<LvkAframe> aframeIter(aniIter.next()._aframes); aframeIter.hasNext();) {
            LvkAframe aframe = aframeIter.next();
            _fpixmaps << QPixmap(fpixmaps.value(aframe.frameId));
            _delays << aframe.delay;
            _oxs << aframe.ox;
            _oys << aframe.oy;
            _stickies << aframe.sticky;
        }
    }

    repaint();
}

void LvkAnimationWidget::setScreenSize(int w, int h) {
    if (w > 0 && h > 0) {
        _scrW = w;
        _scrH = h;
        setGeometry(x(), y(), w + 5, h + 5);
    }

    repaint();
}

void LvkAnimationWidget::clear() {
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

void LvkAnimationWidget::nextFrame() {
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
        if (_currentFrame >= _fpixmaps.size()) {
            _currentFrame = 0;
        }
    }

    repaint();
}

void LvkAnimationWidget::timerEvent(QTimerEvent * /*event*/) {
    killTimer(_currentTimer);
    nextFrame();
    // Only the delay=0 default (and defensive negatives) gets clamped to
    // 16ms. A naive qMax(delay, 16) silently slowed user-authored 1-15ms
    // delays to 16ms (e.g. a 10ms animation ran at ~60% of its intended
    // speed). The original intent was just to avoid startTimer(0), which
    // refires on every event-loop tick and pegs the CPU. Honor positive
    // user delays exactly; substitute 16ms only when the value is non-
    // positive.
    int delayMs = _delays[_currentFrame];
    if (delayMs <= 0) {
        delayMs = 16;  // treat 0 (default for new aframe) as "play as fast as Qt can"
    }
    _currentTimer = startTimer(delayMs);
}

void LvkAnimationWidget::paintEvent(QPaintEvent * /*event*/) {
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

void LvkAnimationWidget::mousePressEvent(QMouseEvent * /*event*/) {
    setCursor(QCursor(Qt::ClosedHandCursor));
}

void LvkAnimationWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton && _currentFrame != -1) {
        // Qt6: QMouseEvent::x()/y() are deprecated; use position() (returns QPointF).
        const QPoint p = event->position().toPoint();
        _origin.setX(p.x() - _fpixmaps[_currentFrame].width() / 2);
        _origin.setY(p.y() - _fpixmaps[_currentFrame].height() / 2);
    } else {
        _origin.setX(1);
        _origin.setY(1);
    }

    repaint();
}

void LvkAnimationWidget::mouseReleaseEvent(QMouseEvent * /*event*/) {
    setCursor(QCursor(Qt::OpenHandCursor));
}

void LvkAnimationWidget::play() {
    if (_fpixmaps.size() > 0) {
        nextFrame();
        // See timerEvent() above: only substitute 16ms when the delay is
        // non-positive (default 0 / defensive negative). Honor positive
        // user-authored delays exactly so a 10ms-delay animation doesn't
        // silently run at 16ms.
        int delayMs = _delays[_currentFrame];
        if (delayMs <= 0) {
            delayMs = 16;  // treat 0 (default for new aframe) as "play as fast as Qt can"
        }
        _currentTimer = startTimer(delayMs);
        _isPlaying = true;
    }

    repaint();
}

void LvkAnimationWidget::stop() {
    killTimer(_currentTimer);
    _currentFrame = -1;
    _currentTimer = 0;
    _isPlaying = false;

    repaint();
}

bool LvkAnimationWidget::isPlaying() {
    return _isPlaying;
}
