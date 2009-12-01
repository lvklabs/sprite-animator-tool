#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <cmath>

#include "qinputimagewidget.h"

// TODO: Clean code!

QInputImageWidget::QInputImageWidget(QWidget *parent)
        : QWidget(parent), _rect(0,0,0,0), _mouseRect(0,0,0,0), _mouseX(-1), _mouseY(-1),
          _rectVisible(true), _mouseLinesVisible(true), _zoom(ZOOM_MIN)
{
    setMouseTracking(true);

    _c = pow(ZOOM_FACTOR, _zoom);
}

void QInputImageWidget::setPixmap(const QPixmap &pixmap)
{
    setFrameRect(pixmap.rect());
    _pixmap = pixmap.isNull() ? pixmap : pixmap.scaled(pixmap.size()*pow(ZOOM_FACTOR, _zoom));
    resize(_pixmap.size());
}

void QInputImageWidget::zoomIn()
{
    if (_zoom < ZOOM_MAX) {
        _zoom++;
        _c = pow(ZOOM_FACTOR, _zoom);
        _scaledRect = rtoz(_rect);
        _mouseRect.setRect(0, 0, 0, 0); // TODO: do not delete mouse rect
        emit mouseRectChanged(ztor(_mouseRect));

        if (!_pixmap.isNull()) {
            _pixmap = _pixmap.scaled(_pixmap.size()*ZOOM_FACTOR);  // TODO: make a cache
        }

        resize(size()*ZOOM_FACTOR);
    }
}

void QInputImageWidget::zoomOut()
{
    if (_zoom > ZOOM_MIN) {
        _zoom--;
        _c = pow(ZOOM_FACTOR, _zoom);
        _scaledRect = rtoz(_rect);
        _mouseRect.setRect(0, 0, 0, 0); // TODO: do not delete mouse rect
        emit mouseRectChanged(ztor(_mouseRect));

        if (!_pixmap.isNull()) {
            _pixmap = _pixmap.scaled(_pixmap.size()/ZOOM_FACTOR); // TODO: make a cache
        }

        resize(size()/ZOOM_FACTOR);
    }
}

QRect QInputImageWidget::ztor(const QRect& rect) const
{
    QRect tmp;
    tmp.setX(ztor(rect.x()));
    tmp.setY(ztor(rect.y()));
    tmp.setWidth(ztor(rect.width()));
    tmp.setHeight(ztor(rect.height()));
    return tmp;
}

QRect QInputImageWidget::rtoz(const QRect& rect) const
{
    QRect tmp;
    tmp.setX(rtoz(rect.x()));
    tmp.setY(rtoz(rect.y()));
    tmp.setWidth(rtoz(rect.width()));
    tmp.setHeight(rtoz(rect.height()));
    return tmp;
}

void QInputImageWidget::setFrameRectVisible(bool visible)
{
    _rectVisible = visible;
    update();
}

bool QInputImageWidget::frameRectVisible() const
{
    return _rectVisible;
}

void QInputImageWidget::setMouseLinesVisible(bool visible)
{
    _mouseLinesVisible = visible;
    update();
}

bool QInputImageWidget::mouseLinesRectVisible() const
{
    return _mouseLinesVisible;
}

void QInputImageWidget::setFrameRect(const QRect &rect)
{
    _rect = rect;
    _scaledRect = rtoz(rect);
    update();
}

const QRect QInputImageWidget::frameRect() const
{
    return _rect;
}

const QRect QInputImageWidget::mouseFrameRect() const
{
    return ztor(_mouseRect);
}

void QInputImageWidget::paintEvent(QPaintEvent */*event*/)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, _pixmap);

    if (_mouseLinesVisible) {
        int mx = pixelate(_mouseX) - 1;
        int my = pixelate(_mouseY) - 1;
        painter.setPen(Qt::gray);
        painter.drawLine(mx, 0,  mx, height());
        painter.drawLine(0, my, width(),my);
    }

    if (_rectVisible) {
        QRect rect;

        rect = _scaledRect;
        rect.setWidth(_scaledRect.width() - 1);
        rect.setHeight(_scaledRect.height() - 1);
        painter.setPen(Qt::red);
        painter.drawRect(rect);

        rect = _mouseRect;
//@ w,h<0
//        if (rect.width() < 0) {
//            rect.setX(rect.x() - 1);
//            rect.setWidth(rect.width() + 1);
//        } else {
//@
            rect.setWidth(rect.width() - 1);
//@ w,h<0
//        }
//        if (rect.height() < 0) {
//            rect.setY(rect.y() - 1);
//            rect.setHeight(rect.height() + 1);
//        } else {
//@
            rect.setHeight(rect.height() - 1);
//@ w,h<0
//        }
//@
        painter.setPen(Qt::black);
        painter.setPen(Qt::DashLine);
        painter.drawRect(rect);
    }
}


void QInputImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        _mouseRect.setX(pixelate(event->x()));
        _mouseRect.setY(pixelate(event->y()));
    } else if (event->buttons() & Qt::RightButton) {
        _mouseRect.setRect(0, 0, 0, 0);
        emit mouseRectChanged(ztor(_mouseRect));

        update();
    }
}

void QInputImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    _mouseX = event->x();
    _mouseY = event->y();

    emit mousePositionChanged(ztor(_mouseX), ztor(_mouseY));

    if (_rectVisible) {
        if (event->buttons() & Qt::LeftButton) {
            int w = _mouseX - _mouseRect.x();
            int h = _mouseY - _mouseRect.y();

            if (w < 0 || h < 0) {
                return;
            }
            _mouseRect.setWidth(pixelate(w));
            _mouseRect.setHeight(pixelate(h));
            emit mouseRectChanged(ztor(_mouseRect));

        }
    }

    update();
}

void QInputImageWidget::mouseReleaseEvent(QMouseEvent */*event*/)
{
}

void QInputImageWidget::resize(const QSize &size)
{
    QWidget::resize(size);
    updateGeometry();
    update();
}

void QInputImageWidget::resize(int w, int h)
{
    resize(QSize(w, h));
}
