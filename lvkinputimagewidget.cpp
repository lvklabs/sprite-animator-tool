#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <cmath>
#include <QApplication>

#include "lvkinputimagewidget.h"

LvkInputImageWidget::LvkInputImageWidget(QWidget *parent)
        : QWidget(parent), _frect(0,0,0,0), _mouseRect(0,0,0,0), _mouseX(-1), _mouseY(-1),
          _frectVisible(true), _mouseLinesVisible(true), _zoom(0)
{
    _c = pow(ZOOM_FACTOR, _zoom);

    _frectPen.setColor(Qt::red);
    _frectPen.setStyle(Qt::SolidLine);
    _mouseRectPen.setColor(Qt::black);
    _mouseRectPen.setStyle(Qt::DashLine);

    setMouseTracking(true);
}

void LvkInputImageWidget::setPixmap(const QPixmap &pixmap)
{
    setFrameRect(pixmap.rect());

    _pixmap = pixmap;

    resize(_pixmap.size()*_c);
}

// TODO: do not delete _mouseRect
#define ZOOM_COMMON() \
            _c = pow(ZOOM_FACTOR, _zoom);\
            _scaledFrect = rtoz(_frect);\
            _mouseRect.setRect(0, 0, 0, 0);\
            resize(_pixmap.size()*_c);\
            emit mouseRectChanged(ztor(_mouseRect));

void LvkInputImageWidget::zoomIn()
{
    if (_zoom < ZOOM_MAX) {
        _zoom++;
        ZOOM_COMMON();
    }
}

void LvkInputImageWidget::zoomOut()
{
    if (_zoom > ZOOM_MIN) {
        _zoom--;
        ZOOM_COMMON();
    }
}

void LvkInputImageWidget::setZoom(int level)
{
    if (level >= ZOOM_MIN && level <= ZOOM_MAX && level != _zoom) {
        _zoom = level;
        ZOOM_COMMON();
    }
}

QRect LvkInputImageWidget::ztor(const QRect& rect) const
{
    QRect tmp;
    tmp.setX(ztor(rect.x()));
    tmp.setY(ztor(rect.y()));
    tmp.setWidth(ztor(rect.width()));
    tmp.setHeight(ztor(rect.height()));
    return tmp;
}

QRect LvkInputImageWidget::rtoz(const QRect& rect) const
{
    QRect tmp;
    tmp.setX(rtoz(rect.x()));
    tmp.setY(rtoz(rect.y()));
    tmp.setWidth(rtoz(rect.width()));
    tmp.setHeight(rtoz(rect.height()));
    return tmp;
}

void LvkInputImageWidget::setFrameRectVisible(bool visible)
{
    _frectVisible = visible;
    update();
}

bool LvkInputImageWidget::frameRectVisible() const
{
    return _frectVisible;
}

void LvkInputImageWidget::setMouseLinesVisible(bool visible)
{
    _mouseLinesVisible = visible;
    update();
}

bool LvkInputImageWidget::mouseLinesRectVisible() const
{
    return _mouseLinesVisible;
}

void LvkInputImageWidget::setFrameRect(const QRect &rect)
{
    _frect = rect;
    _scaledFrect = rtoz(rect);
    update();
}

const QRect LvkInputImageWidget::frameRect() const
{
    return _frect;
}

const QRect LvkInputImageWidget::mouseFrameRect() const
{
    return ztor(_mouseRect);
}

void LvkInputImageWidget::paintEvent(QPaintEvent */*event*/)
{
    QPainter painter(this);

    painter.drawPixmap(0, 0, _pixmap.width()*_c, _pixmap.height()*_c, _pixmap);

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


void LvkInputImageWidget::mousePressEvent(QMouseEvent *event)
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

void LvkInputImageWidget::mouseMoveEvent(QMouseEvent *event)
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

void LvkInputImageWidget::mouseReleaseEvent(QMouseEvent */*event*/)
{
    _mouseRect = _mouseRect.normalized();
    emit mouseRectChanged(ztor(_mouseRect));
    update();
}

void LvkInputImageWidget::wheelEvent(QWheelEvent *event)
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

void LvkInputImageWidget::resize(const QSize &size)
{
    QWidget::resize(size);
    updateGeometry();
    update();
}

void LvkInputImageWidget::resize(int w, int h)
{
    resize(QSize(w, h));
}

LvkInputImageWidget::~LvkInputImageWidget()
{
}
