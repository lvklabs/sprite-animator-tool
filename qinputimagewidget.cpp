#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <cmath>
#include <QApplication>

#include "qinputimagewidget.h"

// TODO: Clean code!

QInputImageWidget::QInputImageWidget(QWidget *parent)
        : QWidget(parent), _frect(0,0,0,0), _mouseRect(0,0,0,0), _mouseX(-1), _mouseY(-1),
          _frectVisible(true), _mouseLinesVisible(true), _zoom(ZOOM_MIN), _pCache(0)
{
    _c      = pow(ZOOM_FACTOR, _zoom);
    _pCache = new QPixmap[ZOOM_MAX + 1];

    _frectPen.setColor(Qt::red);
    _frectPen.setStyle(Qt::SolidLine);
    _mouseRectPen.setColor(Qt::black);
    _mouseRectPen.setStyle(Qt::DashLine);

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
            _scaledFrect = rtoz(_frect);\
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
    _frectVisible = visible;
    update();
}

bool QInputImageWidget::frameRectVisible() const
{
    return _frectVisible;
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
    _frect = rect;
    _scaledFrect = rtoz(rect);
    update();
}

const QRect QInputImageWidget::frameRect() const
{
    return _frect;
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

    if (_mouseLinesVisible && ctrlKeyPressed) {
        int mx;
        int my;

        if (QApplication::mouseButtons() == Qt::LeftButton) {
            mx = _mouseRect.x() + _mouseRect.width() - 1;
            my = _mouseRect.y() + _mouseRect.height() - 1;
        } else {
            mx = pixelate(_mouseX);
            my = pixelate(_mouseY);
        }

        painter.setPen(Qt::gray);
        painter.drawLine(mx, 0,  mx, height());
        painter.drawLine(0, my, width(),my);

        setCursor(QCursor(Qt::BlankCursor));
    } else {
        setCursor(QCursor(Qt::ArrowCursor));
    }

    if (_frectVisible) {
        QRect rect;

        /* draw frame rect */
        rect = _scaledFrect;
        if (!rect.isEmpty()) {
            rect.setWidth(_scaledFrect.width() - 1);
            rect.setHeight(_scaledFrect.height() - 1);
            painter.setPen(_frectPen);
            painter.drawRect(rect);
        }

        /* draw mouse rect */
        rect = _mouseRect.normalized();
        if (!rect.isEmpty()) {
            rect.setWidth(rect.width() - 1);
            rect.setHeight(rect.height() - 1);
            painter.setPen(_mouseRectPen);
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

    if (_frectVisible) {
        if (event->buttons() & Qt::LeftButton) {
            int w = _mouseX - _mouseRect.x();
            int h = _mouseY - _mouseRect.y();

            _mouseRect.setWidth(pixelate(w));
            _mouseRect.setHeight(pixelate(h));

            emit mouseRectChanged(ztor(_mouseRect));
        }
    }

    update();
}

void QInputImageWidget::mouseReleaseEvent(QMouseEvent */*event*/)
{
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
