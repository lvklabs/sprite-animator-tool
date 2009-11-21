#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <cmath>

#include "qinputimagewidget.h"

QInputImageWidget::QInputImageWidget(QWidget *parent)
        : QLabel(parent), _rectVisible(true), _zoom(ZOOM_MIN)
{
    setMouseTracking(true);
}

void QInputImageWidget::setPixmap(const QPixmap &pixmap)
{
    setFrameRect(pixmap.rect());
    _pixmap = pixmap.isNull() ? pixmap : pixmap.scaled(pixmap.size()*pow(ZOOM_FACTOR, _zoom));
    resize(_pixmap.size());
    update();
}

void QInputImageWidget::zoomIn()
{
    if (_zoom < ZOOM_MAX) {
        _zoom++;

        updateScaledRect();

        if (!_pixmap.isNull()) {
            _pixmap = _pixmap.scaled(_pixmap.size()*ZOOM_FACTOR);
        }

        resize(size()*ZOOM_FACTOR);
        update();
    }
}

void QInputImageWidget::zoomOut()
{
    if (_zoom > ZOOM_MIN) {
        _zoom--;

        updateScaledRect();

        if (!_pixmap.isNull()) {
            _pixmap = _pixmap.scaled(_pixmap.size()/ZOOM_FACTOR);
        }

        resize(size()/ZOOM_FACTOR);
        update();
    }
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

void QInputImageWidget::setFrameRect(const QRect &rect)
{
    _rect = rect;
    updateScaledRect();
    update();
}

void QInputImageWidget::updateScaledRect()
{
    int c = pow(ZOOM_FACTOR, _zoom);
    _scaledRect.setX(_rect.x()*c);
    _scaledRect.setY(_rect.y()*c);
    _scaledRect.setWidth(_rect.width()*c);
    _scaledRect.setHeight(_rect.height()*c);

    update();
}

void QInputImageWidget::paintEvent(QPaintEvent */*event*/)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, _pixmap);

    if (_rectVisible) {
        QRect rect = _scaledRect;
        rect.setWidth(_scaledRect.width() - 1);
        rect.setHeight(_scaledRect.height() - 1);
        painter.setPen(Qt::red);
        painter.drawRect(rect);
    }
}

void QInputImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (_rectVisible) {
        int c = pow(ZOOM_FACTOR, _zoom);
        setToolTip(QString::number(event->x()/c) + "," + QString::number(event->y()/c));
    }
}


void QInputImageWidget::resize(const QSize &size)
{
    QLabel::resize(size);
    updateGeometry();
}

void QInputImageWidget::resize(int w, int h)
{
    resize(QSize(w, h));
}
