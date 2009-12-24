#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <cmath>
#include <QApplication>

#include "qinputimagewidget.h"

// TODO: Clean code!

QInputImageWidget::QInputImageWidget(QWidget *parent)
        : QWidget(parent), _rect(0,0,0,0), _mouseRect(0,0,0,0), _mouseX(-1), _mouseY(-1),
          _rectVisible(true), _mouseLinesVisible(true), _zoom(ZOOM_MIN), _pCache(0)
{
    _c      = pow(ZOOM_FACTOR, _zoom);
    _pCache = new QPixmap[ZOOM_MAX + 1];

    setMouseTracking(true);
}

void QInputImageWidget::setPixmap(const QPixmap &pixmap)
{
    setFrameRect(pixmap.rect());

    /* clean pixmap cache */
    _pCache[0] = pixmap;
    for (int i = 1; i < ZOOM_MAX + 1; ++i) {
        _pCache[i] = QPixmap();
    }

    resize(getScaledPixmap().size());
}

QPixmap& QInputImageWidget::getScaledPixmap()
{
    if ( _pCache[0].isNull()) {
        return _pCache[0];
    } else {
        if (_pCache[_zoom].isNull()) {
            _pCache[_zoom] = _pCache[0].scaled(_pCache[0].size()*pow(ZOOM_FACTOR, _zoom));
        }
        return _pCache[_zoom];
    }
}

// TODO: do not delete _mouseRect
#define ZOOM_COMMON() \
            _c = pow(ZOOM_FACTOR, _zoom);\
            _scaledRect = rtoz(_rect);\
            _mouseRect.setRect(0, 0, 0, 0);\
            getScaledPixmap();\
            emit mouseRectChanged(ztor(_mouseRect));

void QInputImageWidget::zoomIn()
{
    if (_zoom < ZOOM_MAX) {
        _zoom++;
        ZOOM_COMMON();
        resize(size()*ZOOM_FACTOR);
    }
}

void QInputImageWidget::zoomOut()
{
    if (_zoom > ZOOM_MIN) {
        _zoom--;
        ZOOM_COMMON();
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
    painter.drawPixmap(0, 0, getScaledPixmap());

    bool ctrlKeyPressed = QApplication::keyboardModifiers() & Qt::ControlModifier;

    if (_mouseLinesVisible && ctrlKeyPressed ) {
        int mx = pixelate(_mouseX) - 1;
        int my = pixelate(_mouseY) - 1;
        painter.setPen(Qt::gray);
        painter.drawLine(mx, 0,  mx, height());
        painter.drawLine(0, my, width(),my);
    }

    if (_rectVisible) {
        QRect rect;

        /* draw frame rect */
        rect = _scaledRect;
        if (!rect.isEmpty()) {
            rect.setWidth(_scaledRect.width() - 1);
            rect.setHeight(_scaledRect.height() - 1);
            painter.setPen(Qt::red);
            painter.drawRect(rect);
        }

        /* draw mouse rect */
        rect = _mouseRect.normalized();
        if (!rect.isEmpty()) {
//        if (rect.width() < 0) {
//            rect.setWidth(rect.width() + 1);
//        } else {
            rect.setWidth(rect.width() - 1);
//        }
//        if (rect.height() < 0) {
//            rect.setHeight(rect.height() + 1);
//        } else {
            rect.setHeight(rect.height() - 1);
//        }
            painter.setPen(Qt::black);
            painter.setPen(Qt::DashLine);
            painter.drawRect(rect);
        }

    }
}


void QInputImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        _mouseRect.setRect(pixelate(event->x()), pixelate(event->y()), 0, 0);
        emit mouseRectChanged(ztor(_mouseRect));
        update();
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

            _mouseRect.setWidth(pixelate(w));
            _mouseRect.setHeight(pixelate(h));

//            if (w < 0) {
//            }
//            if (h < 0) {
//            }

            emit mouseRectChanged(ztor(_mouseRect));
        }
    }

    update();
}

void QInputImageWidget::mouseReleaseEvent(QMouseEvent */*event*/)
{
//    if (_mouseRect.width() < 0) {
//        _mouseRect.setX(_mouseRect.x()+1);
//    }
//    if (_mouseRect.height() < 0) {
//        _mouseRect.setY(_mouseRect.y()+1);
//    }
    _mouseRect = _mouseRect.normalized();
    emit mouseRectChanged(ztor(_mouseRect));
    update();
}

void QInputImageWidget::wheelEvent(QWheelEvent *event)
{
    bool ctrlKeyPressed = QApplication::keyboardModifiers() & Qt::ControlModifier;;
    if (ctrlKeyPressed) {
        if (event->delta() > 0) {
            zoomIn();
        } else if (event->delta() < 0) {
            zoomOut();
        }
        event->accept();
    }
    event->ignore();
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

QInputImageWidget::~QInputImageWidget()
{
    if (_pCache) {
        delete[] _pCache;
    }
}
