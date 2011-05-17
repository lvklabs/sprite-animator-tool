#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QScrollBar>
#include <QDebug>
#include <cmath>

#include "lvkinputimagewidget.h"

QRect operator*(const QRect& rect, int c)
{
    QRect tmp;
    tmp.setX(rect.x()*c);
    tmp.setY(rect.y()*c);
    tmp.setWidth(rect.width()*c);
    tmp.setHeight(rect.height()*c);
    return tmp;
}

QRect operator/(const QRect& rect, int c)
{
    QRect tmp;
    tmp.setX(rect.x()/c);
    tmp.setY(rect.y()/c);
    tmp.setWidth(rect.width()/c);
    tmp.setHeight(rect.height()/c);
    return tmp;
}

LvkInputImageWidget::LvkInputImageWidget(QWidget *parent)
        : QWidget(parent),
        _mouseX(-1), _mouseY(-1), _zoom(0), _scroll(0), _cacheId(NullId)
{
    _c = pow(ZOOM_FACTOR, _zoom);

    for (int i = 0; i < PCACHE_ROW_SIZE; ++i) {
        for (int j = 0; j < PCACHE_COL_SIZE; ++j) {
            _pCache[i][j] = 0;
        }
    }

    setMouseTracking(true);
}

void LvkInputImageWidget::clear()
{
    _pixmap = QPixmap();
    _zoom = 0;
    _c = pow(ZOOM_FACTOR, _zoom);
    clearPixmapCache();
}

void LvkInputImageWidget::clearPixmapCache()
{
    for (int i = 0; i < PCACHE_ROW_SIZE; ++i) {
        clearPixmapCache(i);
    }
    _cacheId = NullId;
}

void LvkInputImageWidget::clearPixmapCache(Id cacheId)
{
    for (int j = 0; j < PCACHE_COL_SIZE; ++j) {
        if (_pCache[cacheId][j]) {
            delete _pCache[cacheId][j];
            _pCache[cacheId][j] = 0;
        }
    }
}

void LvkInputImageWidget::setPixmap(const QPixmap &pixmap, Id useCacheId)
{
    if (useCacheId != NullId) {
        if (useCacheId < 0 || useCacheId >= PCACHE_ROW_SIZE) {
            qDebug() << "WARNING: LvkInputImageWidget::setPixmap() useCacheId out of bunds, using 0" ;
            useCacheId = 0;
        }
        _cacheId = useCacheId;
    }

    _pixmap = pixmap;

    resize(_pixmap.width()*_c + 1, _pixmap.height()*_c + 1);
}

void LvkInputImageWidget::setBlendPixmap(const QPixmap &pixmap)
{
    _blendPixmap = pixmap;
}

void LvkInputImageWidget::setBackground(const QPixmap& bg)
{
    _bg = bg;
    _bgBrush = QBrush(bg);
}


void LvkInputImageWidget::registerRect(QRect * rect)
{
    _regRects.append(rect);
}

void LvkInputImageWidget::unregisterRect(QRect * rect)
{
    _regRects.removeOne(rect);
}

void LvkInputImageWidget::updateRegRects(int level)
{
    int zoom = _zoom;

    if (zoom < level) {
        do {
            for (int i = 0; i < _regRects.size(); ++i) {
                *_regRects.at(i) = *_regRects.at(i)*2;
            }
        } while (++zoom < level);
    } else if (zoom > level) {
        do {
            for (int i = 0; i < _regRects.size(); ++i) {
                *_regRects.at(i) = *_regRects.at(i)/2;
            }
        } while (--zoom > level);
    }
}

#define SET_ZOOM(level)  updateRegRects(level);\
                         _zoom = (level);\
                         _c = pow(ZOOM_FACTOR, _zoom);\
                         resize(_pixmap.width()*_c + 1, _pixmap.height()*_c + 1);

void LvkInputImageWidget::zoomIn()
{
    if (_zoom < ZOOM_MAX) {
        SET_ZOOM(_zoom + 1);
    }
}

void LvkInputImageWidget::zoomOut()
{
    if (_zoom > ZOOM_MIN) {
        SET_ZOOM(_zoom - 1);
    }
}

void LvkInputImageWidget::setZoom(int level)
{
    if (level >= ZOOM_MIN && level <= ZOOM_MAX && level != _zoom) {
       SET_ZOOM(level);
    }
}

int LvkInputImageWidget::zoom()
{
    return _zoom;
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

void LvkInputImageWidget::setScrollArea(QScrollArea *scroll)
{
    _scroll = scroll;
}

void LvkInputImageWidget::fillBackground(QPainter& painter, int x, int y, int w, int h)
{
    if (!_bg.isNull()) {
        if (w >= width()) {
            w = width() - 1;
        }
        if (h >= height()) {
            h = height() - 1;
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(_bgBrush);
        painter.drawRect(x, y, w, h);
        painter.setBrush(Qt::NoBrush);
    }
}

void LvkInputImageWidget::paintEvent(QPaintEvent */*event*/)
{
    QPainter painter(this);

    if (_pixmap.isNull()) {
        return;
    }

    if (_scroll) {
        /* optimized draw Image*/

        int hval = _scroll->horizontalScrollBar()->value();
        int vval = _scroll->verticalScrollBar()->value();
        int w = _scroll->width();
        int h = _scroll->height();

        painter.setClipping(true);
        painter.setClipRect(hval, vval, w, h);

        fillBackground(painter, hval, vval, w, h);

        int z = _zoom >= 0 ? _zoom : ZOOM_MAX - _zoom;

        if (!_blendPixmap.isNull()) {
            painter.setOpacity(0.5);
            painter.drawPixmap(0, 0, _blendPixmap.width()*_c, _blendPixmap.height()*_c, _blendPixmap);
        }

        if (_cacheId != NullId) {
            if (!_pCache[_cacheId][z]) {
                _pCache[_cacheId][z] = new QPixmap();
                *_pCache[_cacheId][z] = _pixmap.scaled(_pixmap.width()*_c, _pixmap.height()*_c);
            }
            painter.drawPixmap(hval, vval, w, h, *_pCache[_cacheId][z], hval, vval, w, h);
        } else {
            painter.drawPixmap(hval, vval, w, h,
                               _pixmap.scaled(_pixmap.width()*_c, _pixmap.height()*_c),
                               hval, vval, w, h);
        }
    } else {
        if (!_blendPixmap.isNull()) {
            painter.setOpacity(0.5);
            painter.drawPixmap(0, 0, _blendPixmap.width()*_c, _blendPixmap.height()*_c, _blendPixmap);
        }

        painter.drawPixmap(0, 0, _pixmap.width()*_c, _pixmap.height()*_c, _pixmap);
    }
}

void LvkInputImageWidget::mousePressEvent(QMouseEvent */*event*/)
{
}

void LvkInputImageWidget::mouseReleaseEvent(QMouseEvent */*event*/)
{
}


void LvkInputImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    _mouseX = event->x();
    _mouseY = event->y();

    emit mousePositionChanged(ztor(_mouseX), ztor(_mouseY));
}

void LvkInputImageWidget::wheelEvent(QWheelEvent *event)
{
    if (ctrlKey()) {
        if (event->delta() > 0) {
            zoomIn();
        } else if (event->delta() < 0) {
            zoomOut();
        }
        event->accept();
    }
    event->ignore();
}

void LvkInputImageWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_1) {
        setZoom(0);
    }
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
    clearPixmapCache();
}
