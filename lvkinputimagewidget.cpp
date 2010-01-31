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
        : QWidget(parent), _frect(0,0,0,0), _mouseRect(0,0,0,0), _mouseX(-1), _mouseY(-1),
          _frectVisible(true), _mouseLinesVisible(true), _zoom(0), _draggingRect(false),
          _resizingRect(false), _hGuide(true), _cacheId(NullId), _scroll(0)
{
    _c = pow(ZOOM_FACTOR, _zoom);

    _frectPen.setColor(QColor(255 , 127, 127));
    _frectPen.setStyle(Qt::SolidLine);
    _mouseRectPen.setColor(Qt::black);
    _mouseRectPen.setStyle(Qt::DashLine);
    _mouseGuidePen.setColor(Qt::gray);
    _mouseGuidePen.setStyle(Qt::SolidLine);
    _guidePen.setColor(QColor(127 , 127, 255));
    _guidePen.setStyle(Qt::DashLine);

    for (int i = 0; i < PCACHE_ROW_SIZE; ++i) {
        for (int j = 0; j < PCACHE_COL_SIZE; ++j) {
            _pCache[i][j] = 0;
        }
    }

    setMouseTracking(true);
}

void LvkInputImageWidget::clear()
{
    _frect = QRect(0,0,0,0);
    _mouseRect = QRect(0,0,0,0);
    _pixmap = QPixmap();
    _zoom = 0;
    _c = pow(ZOOM_FACTOR, _zoom);
    _hGuide = true;
    clearPixmapCache();

    emit mouseRectChanged(ztor(_mouseRect));
}

void LvkInputImageWidget::clearPixmapCache()
{
    _cacheId = NullId;
    for (int i = 0; i < PCACHE_ROW_SIZE; ++i) {
        for (int j = 0; j < PCACHE_COL_SIZE; ++j) {
            if (_pCache[i][j]) {
                delete _pCache[i][j];
                _pCache[i][j] = 0;
            }
        }
    }
}

void LvkInputImageWidget::setPixmap(const QPixmap &pixmap, Id useCacheId)
{
    setFrameRect(pixmap.rect());

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

#define ZOOM_COMMON() \
            _c = pow(ZOOM_FACTOR, _zoom);\
            _scaledFrect = rtoz(_frect);\
            resize(_pixmap.size()*_c);\
            emit mouseRectChanged(ztor(_mouseRect));

void LvkInputImageWidget::zoomIn()
{
    if (_zoom < ZOOM_MAX) {
        _zoom++;
        _mouseRect = _mouseRect*2;
        ZOOM_COMMON();
    }
}

void LvkInputImageWidget::zoomOut()
{
    if (_zoom > ZOOM_MIN) {
        _zoom--;
        _mouseRect = _mouseRect/2;
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

void LvkInputImageWidget::clearGuides()
{
    _guides.clear();
    update();
}

void LvkInputImageWidget::setScrollArea(QScrollArea *scroll)
{
    _scroll = scroll;
}

bool LvkInputImageWidget::mouseCrossGuidesMode()
{
    return _mouseLinesVisible && !_draggingRect && ctrlKey();
}

bool LvkInputImageWidget::mouseBlueGuideMode()
{
    return _mouseLinesVisible && !_resizingRect && !_draggingRect && ctrlKey() && shiftKey();
}

bool LvkInputImageWidget::canDrag()
{
    return !_resizingRect && _mouseRect.isValid() && _mouseRect.contains(_mouseX, _mouseY) && !mouseBlueGuideMode();
}

void LvkInputImageWidget::paintEvent(QPaintEvent */*event*/)
{
    QPainter painter(this);

    /* set mouse cursor */
    if (mouseCrossGuidesMode() || mouseBlueGuideMode()) {
        setCursor(QCursor(Qt::BlankCursor));
    } else if (_draggingRect || canDrag()) {
        setCursor(QCursor(Qt::SizeAllCursor));
    } else {
        setCursor(QCursor(Qt::ArrowCursor));
    }

    if (_scroll) {
        /* optimized draw Image*/
        int hval = _scroll->horizontalScrollBar()->value();
        int vval = _scroll->verticalScrollBar()->value();
        int w = _scroll->width();
        int h = _scroll->height();

        painter.setClipping(true);
        painter.setClipRect(hval, vval, w, h);

        int z = _zoom >= 0 ? _zoom : ZOOM_MAX - _zoom;

        if (_cacheId != NullId) {
            if (!_pCache[_cacheId][z]) {
                _pCache[_cacheId][z] = new QPixmap();
                if (!_pixmap.isNull()) {
                    *_pCache[_cacheId][z] = _pixmap.scaled(_pixmap.width()*_c, _pixmap.height()*_c);
                }
            }
            painter.drawPixmap(hval, vval, w, h, *_pCache[_cacheId][z], hval, vval, w, h);
        } else {
            painter.drawPixmap(hval, vval, w, h, _pixmap.scaled(_pixmap.width()*_c, _pixmap.height()*_c), hval, vval, w, h);
        }
    } else {
        /* draw Image*/
        painter.drawPixmap(0, 0, _pixmap.width()*_c, _pixmap.height()*_c, _pixmap);
    }


    if (_mouseLinesVisible) {
        /* draw guide lines */
        painter.setPen(_guidePen);
        for (int i = 0; i < _guides.size(); ++i) {
            int x = rtoz(_guides.at(i).x());
            int y = rtoz(_guides.at(i).y());
            painter.drawLine(x, 0,  x, height());
            painter.drawLine(0, y, width(),y);
        }
    }

    /* draw mouse guides */
    if (mouseCrossGuidesMode()) {
        int mx = pixelate(_mouseX);
        int my = pixelate(_mouseY);

        // TODO check why this works!
        if (_mouseRect.width() < 0) {
            mx = pixelate(_mouseX - 1) + _c;
        }
        if (_mouseRect.height() < 0) {
            my = pixelate(_mouseY - 1) + _c;
        }

        if (mouseBlueGuideMode()) {
            painter.setPen(_guidePen);
            if (_hGuide) {
                painter.drawLine(0, my, width(),my);
            } else {
                painter.drawLine(mx, 0,  mx, height());
            }
        } else {
            painter.setPen(_mouseGuidePen);
            painter.drawLine(mx, 0,  mx, height());
            painter.drawLine(0, my, width(),my);
        }
    }

    if (_frectVisible) {
        /* draw frame rect */
        if (!_scaledFrect.isEmpty()) {
            painter.setPen(_frectPen);
            painter.drawRect(_scaledFrect);
        }

        /* draw mouse rect */
        QRect rect = _mouseRect.normalized();
        if (!rect.isEmpty()) {
            painter.setPen(_mouseRectPen);
            painter.drawRect(rect);
        }
    }
}


void LvkInputImageWidget::mousePressEvent(QMouseEvent *event)
{
    _mouseClickX = event->x();
    _mouseClickY = event->y();

    if (event->buttons() & Qt::LeftButton) {
        if (canDrag()) {
            _draggingRect = true;
            _mouseRectP = _mouseRect;
        } else if (mouseBlueGuideMode()) {
            if (_hGuide) {
                _guides.append(QPoint(-1, ztor(pixelate(_mouseClickY))));
            } else {
                _guides.append(QPoint(ztor(pixelate(_mouseClickX)), -1));
            }
        } else {
            _mouseRect.setRect(pixelate(_mouseClickX), pixelate(_mouseClickY), 0, 0);
            _resizingRect = true;
            emit mouseRectChanged(ztor(_mouseRect));
        }
    } else if (event->buttons() & Qt::RightButton) {
        if (mouseBlueGuideMode()) {
            _hGuide = !_hGuide;
//        } else if (_draggingRect) {
//            _mouseRect = _mouseRectP;
//            _draggingRect = false;
//            emit mouseRectChanged(ztor(_mouseRect));
        } else {
            _mouseRect.setRect(0, 0, 0, 0);
            emit mouseRectChanged(ztor(_mouseRect));
        }
    }
    update();
}

void LvkInputImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    _mouseX = event->x();
    _mouseY = event->y();

    emit mousePositionChanged(ztor(_mouseX), ztor(_mouseY));


    if (_frectVisible) {
        if (_draggingRect) {
            int dx = _mouseX -_mouseClickX;
            int dy = _mouseY -_mouseClickY;
            if (!ctrlKey()) {
                _mouseRect.moveTo(pixelate(_mouseRectP.x() + dx),
                                  pixelate(_mouseRectP.y() + dy));
            } else {
                if (abs(dx) > 20 ) {
                    _mouseRect.moveTo(pixelate(_mouseRectP.x() + dx),
                                      pixelate(_mouseRectP.y()));
                } else {
                    _mouseRect.moveTo(pixelate(_mouseRectP.x()),
                                      pixelate(_mouseRectP.y() + dy));
                }
            }
            emit mouseRectChanged(ztor(_mouseRect));
        } else if (_resizingRect && event->buttons() & Qt::LeftButton) {
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
    _draggingRect = false;
    _resizingRect = false;
    _mouseRect = _mouseRect.normalized();
    emit mouseRectChanged(ztor(_mouseRect));
    update();
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

void LvkInputImageWidget::scrollToFrame(const LvkFrame& frame)
{
    if (!_scroll) {
        return;
    }

    int margin = 10;

    // TODO simplify

    int hval = _scroll->horizontalScrollBar()->value();
    int hmax = _scroll->horizontalScrollBar()->maximum();
    int w    = width();                     /* img width total */
    int wv   = w - hmax + hval;             /* img width visible */
    int fx1 = frame.ox * _c;                /* frame rect x1 */
    int fx2 = (frame.ox + frame.w) * _c;    /* frame rect x2 */

    if (fx1 <= hval) {
        _scroll->horizontalScrollBar()->setValue(fx1 - margin);
    } else if (fx2 >= wv) {
        _scroll->horizontalScrollBar()->setValue(hval + (fx2 - wv) + margin);
    }

    int vval = _scroll->verticalScrollBar()->value();
    int vmax = _scroll->verticalScrollBar()->maximum();
    int h    = height();                    /* img height total */
    int hv   = h - vmax + vval;             /* img height visible */
    int fy1 = frame.oy * _c;                /* frame rect y1 */
    int fy2 = (frame.oy + frame.h) * _c;    /* frame rect y1 */

    if (fy1 <= vval) {
        _scroll->verticalScrollBar()->setValue(fy1 - margin);
    } else if (fy2 >= hv) {
        _scroll->verticalScrollBar()->setValue(vval + (fy2 - hv) + margin);
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
