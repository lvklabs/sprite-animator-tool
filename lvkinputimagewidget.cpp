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
        : QWidget(parent), _frect(0,0,0,0), _mouseRect(0,0,0,0), _activeRect(0), _mouseX(-1), _mouseY(-1),
          _frectVisible(true), _mouseLinesVisible(true), _zoom(0), _draggingRect(false),
          _hGuide(true), _cacheId(NullId), _scroll(0)
{
    _c = pow(ZOOM_FACTOR, _zoom);

    _frectPen.setColor(QColor(255, 100, 100));
    _frectPen.setStyle(Qt::SolidLine);
    _mouseRectPen.setColor(Qt::black);
    _mouseRectPen.setStyle(Qt::DashLine);
    _mouseGuidePen.setColor(Qt::gray);
    _mouseGuidePen.setStyle(Qt::SolidLine);
    _guidePen.setColor(QColor(127 , 127, 255));
    _guidePen.setStyle(Qt::DashLine);
    _resizeControlsPen.setColor(QColor(180, 180, 180));
    _resizeControlsPen.setStyle(Qt::SolidLine);

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
    _pixmap = QPixmap();
    _zoom = 0;
    _c = pow(ZOOM_FACTOR, _zoom);
    _hGuide = true;
    clearPixmapCache();

    if (!_mouseRect.isNull()) {
        _mouseRect = QRect(0,0,0,0);
        emit mouseRectChangeFinished(ztor(_mouseRect));
    }
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

void LvkInputImageWidget::setBackground(const QPixmap& bg)
{
    _bg = bg;
    _bgBrush = QBrush(bg);
}

#define ZOOM_COMMON() \
            _c = pow(ZOOM_FACTOR, _zoom);\
            _scaledFrect = rtoz(_frect);\
            resize(_pixmap.width()*_c + 1, _pixmap.height()*_c + 1);\
            emit mouseRectChangeFinished(ztor(_mouseRect));

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

inline bool LvkInputImageWidget::mouseCrossGuidesMode() const
{
    return _mouseLinesVisible && !_draggingRect && ctrlKey();
}

inline bool LvkInputImageWidget::mouseBlueGuideMode() const
{
    return _mouseLinesVisible && !_resizingRect && !_draggingRect && ctrlKey() && shiftKey();
}

inline bool LvkInputImageWidget::isMouseOver(const QRect& rect) const
{
    return rect.contains(_mouseX, _mouseY);
}

// I Know... I abuse of macros but believe is a quite nice solution :)
//                                                           andres

#define INIT_RECT_SIZES(rect)   int x  = rect.x();\
                                int y  = rect.y();\
                                int w  = rect.width();\
                                int h  = rect.height();\
                                int l  = RESIZE_CONTROL_SIZE;\
                                int ll = 2*l;

#define INIT_RECT_SIZES2(rect)  int w  = rect.width();\
                                int h  = rect.height();\
                                int l  = RESIZE_CONTROL_SIZE;\
                                int ll = 2*l;


/* if the mouse rect is big enough, draw rects inside the frame, otherwise draw outside*/
#define DrawResizeControlsInside       (w >= l*3 && h >= l*3)
#define RectTop          (DrawResizeControlsInside ? QRect(x + l, y, w - ll, l)         : QRect(x, y - l, w, l))
#define RectTopRight     (DrawResizeControlsInside ? QRect(x + w - l, y, l, l)          : QRect(x + w, y - l, l, l))
#define RectRight        (DrawResizeControlsInside ? QRect(x + w - l, y + l, l, h - ll) : QRect(x + w, y, l, h))
#define RectBottomRight  (DrawResizeControlsInside ? QRect(x + w - l, y + h - l, l, l)  : QRect(x + w, y + h, l, l))
#define RectBottom       (DrawResizeControlsInside ? QRect(x + l, y + h - l, w - ll, l) : QRect(x, y + h, w, l))
#define RectBottomLeft   (DrawResizeControlsInside ? QRect(x, y + h - l, l, l)          : QRect(x - l, y + h, l, l))
#define RectLeft         (DrawResizeControlsInside ? QRect(x, y + l, l, h - ll)         : QRect(x - l, y, l, h))
#define RectTopLeft      (DrawResizeControlsInside ? QRect(x, y, l, l)                  : QRect(x - l, y - l, l, l))


const QRect* LvkInputImageWidget::mouseOverRect(bool withResizeControls) const
{
    if (withResizeControls) {
        if (isMouseOverResizeControls(_mouseRect)) {
            return &_mouseRect;
        } else if (isMouseOverResizeControls(_scaledFrect)) {
            return &_scaledFrect;
        } else {
            return 0;
        }
    } else {
        if (_mouseRect.contains(_mouseX, _mouseY)) {
            return &_mouseRect;
        } else if (_scaledFrect.contains(_mouseX, _mouseY)) {
            return &_scaledFrect;
        } else {
            return 0;
        }
    }
}

bool LvkInputImageWidget::canDrag() const
{
    const QRect* pRect = mouseOverRect();

    return /*!_resizingRect &&*/ pRect && !(*pRect).isNull() && !mouseBlueGuideMode();
}

bool LvkInputImageWidget::canResize(ResizeType type) const
{
    const QRect* pRect = mouseOverRect();
    
    if (!pRect) {
        return false;
    }

    QRect rect = (*pRect).normalized();

    if (rect.isEmpty()) {
        return false;
    }

    INIT_RECT_SIZES(rect);

    switch (type) {
    case ResizeNull:
        return false;
    case ResizeTop:
        return RectTop.contains(_mouseX, _mouseY);
    case ResizeTopRight:
        return RectTopRight.contains(_mouseX, _mouseY);
    case ResizeRight:
        return RectRight.contains(_mouseX, _mouseY);
    case ResizeBottomRight:
        return RectBottomRight.contains(_mouseX, _mouseY);
    case ResizeBottom:
        return RectBottom.contains(_mouseX, _mouseY);
    case ResizeBottomLeft:
        return RectBottomLeft.contains(_mouseX, _mouseY);
    case ResizeLeft:
        return RectLeft.contains(_mouseX, _mouseY);
    case ResizeTopLeft:
        return RectTopLeft.contains(_mouseX, _mouseY);
    default:
        return false;
    }
}

bool LvkInputImageWidget::isMouseOverResizeControls(const QRect& rect) const
{
    if (rect.isEmpty()) {
        return false;
    }

    INIT_RECT_SIZES2(rect);

    if (DrawResizeControlsInside) {
        return rect.contains(_mouseX, _mouseY);
    } else {
        QRect tmp(rect.x() - l, rect.y() - l, rect.width() + ll, rect.height() + ll);
        return tmp.contains(_mouseX, _mouseY);
    }
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

    setMouseCursor();

    paintImage(painter);
    paintGuides(painter);
    paintMouseGuides(painter);
    paintFrameRect(painter);
    paintMouseRect(painter);
}

void LvkInputImageWidget::paintImage(QPainter& painter)
{
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

        if (_cacheId != NullId) {
            if (!_pCache[_cacheId][z]) {
                _pCache[_cacheId][z] = new QPixmap();
                *_pCache[_cacheId][z] = _pixmap.scaled(_pixmap.width()*_c, _pixmap.height()*_c);
            }
            painter.drawPixmap(hval, vval, w, h, *_pCache[_cacheId][z], hval, vval, w, h);
        } else {
            fillBackground(painter, 0, 0, width(), height());
            painter.drawPixmap(hval, vval, w, h,
                               _pixmap.scaled(_pixmap.width()*_c, _pixmap.height()*_c),
                               hval, vval, w, h);
        }
    } else {
        painter.drawPixmap(0, 0, _pixmap.width()*_c, _pixmap.height()*_c, _pixmap);
    }
}

void LvkInputImageWidget::paintGuides(QPainter& painter)
{
    if (!_mouseLinesVisible) {
        return;
    }

    painter.setPen(_guidePen);
    for (int i = 0; i < _guides.size(); ++i) {
        int x = rtoz(_guides.at(i).x());
        int y = rtoz(_guides.at(i).y());
        painter.drawLine(x, 0,  x, height());
        painter.drawLine(0, y, width(),y);
    }
}

void LvkInputImageWidget::paintMouseGuides(QPainter& painter)
{
    if (mouseCrossGuidesMode() && underMouse()) {
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
}

void LvkInputImageWidget::paintFrameRect(QPainter& painter)
{
    if (!_frectVisible) {
        return;
    }

    QRect rect = _scaledFrect.normalized();

    if (!rect.isEmpty()) {
        if (mouseOverRect() != &_mouseRect) {
            paintResizeControls(painter, rect);
        }
        painter.setPen(_frectPen);
        painter.drawRect(rect);
    }
}

void LvkInputImageWidget::paintMouseRect(QPainter& painter)
{
    if (!_frectVisible) {
        return;
    }

    QRect rect = _mouseRect.normalized();

    if (!rect.isEmpty()) {
        paintResizeControls(painter, rect);
        painter.setPen(_mouseRectPen);
        painter.drawRect(rect);
    }

}

void LvkInputImageWidget::paintResizeControls(QPainter &painter, const QRect &rect)
{
    if (!underMouse()) {
        return;
    }

    bool drawResizeControls = isMouseOverResizeControls(rect) &&
                           !_draggingRect &&
                           !_resizingRect &&
                           !mouseBlueGuideMode();

    if (drawResizeControls) {
        INIT_RECT_SIZES(rect);

        painter.setPen(_resizeControlsPen);
        painter.drawRect(RectTop);
        painter.drawRect(RectTopRight);
        painter.drawRect(RectRight);
        painter.drawRect(RectBottomRight);
        painter.drawRect(RectBottom);
        painter.drawRect(RectBottomLeft);
        painter.drawRect(RectLeft);
        painter.drawRect(RectTopLeft);
    }
}

void LvkInputImageWidget::mousePressEvent(QMouseEvent *event)
{
    _mouseClickX = event->x();
    _mouseClickY = event->y();

    if (event->buttons() & Qt::LeftButton) {
        mousePressLeftButtonEvent(event);
    } else if (event->buttons() & Qt::RightButton) {
        mousePressRightButtonEvent(event);
    }

    update();
}

void LvkInputImageWidget::mousePressLeftButtonEvent(QMouseEvent */*event*/)
{
    _activeRect = const_cast<QRect*>(mouseOverRect());

    if (canResize(ResizeTop)) {
        _resizingRect = ResizeTop;
    } else if (canResize(ResizeTopRight)) {
        _resizingRect = ResizeTopRight;
    } else if (canResize(ResizeRight)) {
        _resizingRect = ResizeRight;
    } else if (canResize(ResizeBottomRight)) {
        _resizingRect = ResizeBottomRight;
    } else if (canResize(ResizeBottom)) {
        _resizingRect = ResizeBottom;
    } else if (canResize(ResizeBottomLeft)) {
        _resizingRect = ResizeBottomLeft;
    } else if (canResize(ResizeLeft)) {
        _resizingRect = ResizeLeft;
    } else if (canResize(ResizeTopLeft)) {
        _resizingRect = ResizeTopLeft;
    } else if (canDrag()) {
        _draggingRect = true;
    } else if (mouseBlueGuideMode()) {
        if (_hGuide) {
            _guides.append(QPoint(-1, ztor(pixelate(_mouseClickY))));
        } else {
            _guides.append(QPoint(ztor(pixelate(_mouseClickX)), -1));
        }
    } else {
        /* creating new rect */
        _mouseRect.setRect(pixelate(_mouseClickX), pixelate(_mouseClickY), 0, 0);
        _resizingRect = ResizeBottomRight;
        emit mouseRectChangeFinished(ztor(_mouseRect));
    }

    if (_activeRect) {
        _mouseRectP = *_activeRect;
    } else {
        _mouseRectP = QRect();
    }
}

void LvkInputImageWidget::mousePressRightButtonEvent(QMouseEvent */*event*/)
{
    if (mouseBlueGuideMode()) {
        _hGuide = !_hGuide;
//        } else if (_draggingRect) {
//            _mouseRect = _mouseRectP;
//            _draggingRect = false;
//            emit mouseRectChangeFinished(ztor(_mouseRect));
    } else {
        _mouseRect.setRect(0, 0, 0, 0);
        emit mouseRectChangeFinished(ztor(_mouseRect));
    }
}

void LvkInputImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    _mouseX = event->x();
    _mouseY = event->y();

    emit mousePositionChanged(ztor(_mouseX), ztor(_mouseY));

    if (event->buttons() & Qt::LeftButton) {
        mouseMoveUpdateRects();
    }

    update();
}

void LvkInputImageWidget::mouseMoveUpdateRects()
{
    if (!_frectVisible) {
        return;
    }

    int dx = _mouseX - _mouseClickX;
    int dy = _mouseY - _mouseClickY;

    bool newMouseRect = false;

    if (!_activeRect) {
        newMouseRect = true;
        _activeRect = &_mouseRect;
    }

    QRect& rect = *_activeRect;

    if (_resizingRect) {
        if (_resizingRect & ResizeTop) {
            int y = _mouseRectP.y() + dy ;
            rect.setY(pixelate(y));
        }
        if (_resizingRect & ResizeRight) {
            int w = (newMouseRect) ? _mouseX - _mouseRect.x() : _mouseRectP.width() + dx;
            rect.setWidth(pixelate(w));
        }
        if (_resizingRect & ResizeBottom) {
            int h = (newMouseRect) ? _mouseY - _mouseRect.y() : _mouseRectP.height() + dy;
            rect.setHeight(pixelate(h));
        }
        if (_resizingRect & ResizeLeft) {
            int x = _mouseRectP.x() + dx ;
            rect.setX(pixelate(x));
        }
    } else if (_draggingRect) {
        if (!ctrlKey()) {
            rect.moveTo(pixelate(_mouseRectP.x() + dx), pixelate(_mouseRectP.y() + dy));
        } else {
            if (abs(dx) > 20) {
                rect.moveTo(pixelate(_mouseRectP.x() + dx), pixelate(_mouseRectP.y()));
            } else {
                rect.moveTo(pixelate(_mouseRectP.x()), pixelate(_mouseRectP.y() + dy));
            }
        }
    }

    emitRectChanging();
}

void LvkInputImageWidget::mouseReleaseEvent(QMouseEvent */*event*/)
{
    _mouseRect = _mouseRect.normalized();
    _scaledFrect = _scaledFrect.normalized();
    _draggingRect = false;
    _resizingRect = ResizeNull;

    emitRectChangeFinished();

    _activeRect = 0; /* Important: do this *after* calling emitRectChangeFinished */

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

void LvkInputImageWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_1) {
        setZoom(0);
    }
}

void LvkInputImageWidget::setMouseCursor()
{
    if (mouseCrossGuidesMode() || mouseBlueGuideMode()) {
        setCursor(QCursor(Qt::BlankCursor));
    } else if (canResize(ResizeTop) || canResize(ResizeBottom) ||
               _resizingRect == ResizeTop || _resizingRect == ResizeBottom) {
        setCursor(QCursor(Qt::SizeVerCursor));
    } else if (canResize(ResizeRight) || canResize(ResizeLeft) ||
                _resizingRect == ResizeRight || _resizingRect == ResizeLeft) {
        setCursor(QCursor(Qt::SizeHorCursor));
    } else if (canResize(ResizeTopRight) || canResize(ResizeBottomLeft) ||
                _resizingRect == ResizeTopRight || _resizingRect == ResizeBottomLeft) {
        setCursor(QCursor(Qt::SizeBDiagCursor));
    } else if (canResize(ResizeBottomRight) || canResize(ResizeTopLeft) ||
                _resizingRect == ResizeBottomRight || _resizingRect == ResizeTopLeft) {
        setCursor(QCursor(Qt::SizeFDiagCursor));
    } else if (canDrag() || _draggingRect) {
        setCursor(QCursor(Qt::SizeAllCursor));
    } else {
        setCursor(QCursor(Qt::ArrowCursor));
    }
}

void LvkInputImageWidget::scrollToFrame(const LvkFrame& frame)
{
    if (!_scroll) {
        return;
    }
    // TODO simplify

    int margin = 10;

    /* Horizontal scroll */

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

    /* Vertical scroll */

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

void LvkInputImageWidget::emitRectChanging()
{
    if (_activeRect == &_mouseRect) {
        emit mouseRectChanging(ztor(_mouseRect));
    } else if (_activeRect == &_scaledFrect) {
        emit frameRectChanging(ztor(_scaledFrect));
    }
}

void LvkInputImageWidget::emitRectChangeFinished()
{
    if (_activeRect == &_mouseRect) {
        emit mouseRectChangeFinished(ztor(_mouseRect));
    } else if (_activeRect == &_scaledFrect) {
        emit frameRectChangeFinished(ztor(_scaledFrect));
    }
}

