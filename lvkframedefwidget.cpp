#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QScrollBar>
#include <QDebug>
#include <cmath>

#include "lvkframedefwidget.h"

LvkFrameDefWidget::LvkFrameDefWidget(QWidget *parent)
        : LvkInputImageWidget(parent), _frect(0,0,0,0), _mrect(0,0,0,0),
          _activeRect(0), _draggingRect(false), _hGuide(true)
{
    registerRect(&_frect);
    registerRect(&_mrect);

    _frectPen.setColor(QColor(255, 100, 100));
    _frectPen.setStyle(Qt::SolidLine);
    _mrectPen.setColor(Qt::black);
    _mrectPen.setStyle(Qt::DashLine);
    _mouseGuidePen.setColor(Qt::gray);
    _mouseGuidePen.setStyle(Qt::SolidLine);
    _guidePen.setColor(QColor(0, 0, 255));
    _guidePen.setStyle(Qt::DashLine);
    _resizeControlsPen.setColor(QColor(180, 180, 180));
    _resizeControlsPen.setStyle(Qt::SolidLine);

    setMouseTracking(true);
}

void LvkFrameDefWidget::clear()
{
    _frect = QRect(0,0,0,0);
    _hGuide = true;

    if (!_mrect.isNull()) {
        _mrect = QRect(0,0,0,0);
        emit mouseRectChangeFinished(ztor(_mrect));
    }
    
    LvkInputImageWidget::clear();
}

void LvkFrameDefWidget::setFrameRect(const QRect &rect)
{
    _frect = rtoz(rect);
    update();
}

const QRect LvkFrameDefWidget::frameRect() const
{
    return ztor(_frect);
}

const QRect LvkFrameDefWidget::mouseFrameRect() const
{
    return ztor(_mrect);
}

void LvkFrameDefWidget::clearGuides()
{
    _guides.clear();
    update();
}

inline bool LvkFrameDefWidget::mouseCrossGuidesMode() const
{
    return !_draggingRect && ctrlKey();
}

inline bool LvkFrameDefWidget::mouseBlueGuideMode() const
{
    return !_resizingRect && !_draggingRect && ctrlKey() && shiftKey();
}

inline bool LvkFrameDefWidget::isMouseOver(const QRect& rect) const
{
    return rect.contains(_mouseX, _mouseY);
}

// I Know... I abuse of macros but believe is a quite nice solution :)
// The best solution could be to define rects as custom widgets that are
// children of this widget
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


const QRect* LvkFrameDefWidget::mouseOverRect(bool withResizeControls) const
{
    if (withResizeControls) {
        if (isMouseOverResizeControls(_mrect)) {
            return &_mrect;
        } else if (isMouseOverResizeControls(_frect)) {
            return &_frect;
        } else {
            return 0;
        }
    } else {
        if (_mrect.contains(_mouseX, _mouseY)) {
            return &_mrect;
        } else if (_frect.contains(_mouseX, _mouseY)) {
            return &_frect;
        } else {
            return 0;
        }
    }
}

bool LvkFrameDefWidget::canDrag() const
{
    const QRect* pRect = mouseOverRect();

    return /*!_resizingRect &&*/ pRect && !(*pRect).isNull() && !mouseBlueGuideMode();
}

bool LvkFrameDefWidget::canResize(ResizeType type) const
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

bool LvkFrameDefWidget::isMouseOverResizeControls(const QRect& rect) const
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

void LvkFrameDefWidget::paintEvent(QPaintEvent *event)
{
    LvkInputImageWidget::paintEvent(event);

    QPainter painter(this);

    setMouseCursor();

    paintGuides(painter);
    paintFrameRect(painter);

    if (_resizingRect) {
        paintMouseGuides(painter);
        paintMouseRect(painter);
    } else {
        paintMouseRect(painter);
        paintMouseGuides(painter);
    }
}

void LvkFrameDefWidget::paintGuides(QPainter& painter)
{
    painter.setPen(_guidePen);
    for (int i = 0; i < _guides.size(); ++i) {
        int x = rtoz(_guides.at(i).x());
        int y = rtoz(_guides.at(i).y());
        painter.drawLine(x, 0,  x, height());
        painter.drawLine(0, y, width(),y);
    }
}

void LvkFrameDefWidget::paintMouseGuides(QPainter& painter)
{
    if (mouseCrossGuidesMode() && underMouse()) {
        int mx;
        int my;

        if (_resizingRect) {
            mx = _mrect.x() + _mrect.width();
            my = _mrect.y() + _mrect.height();

            if (_mrect.width() < 0) {
                mx--;
            }
            if (_mrect.height() < 0) {
                my--;
            }
        } else {
            mx = pixelate(_mouseX);
            my = pixelate(_mouseY);

//            if (_mrect.width() < 0) {
//                mx = pixelate(_mouseX - 1) + _c;
//            }
//            if (_mrect.height() < 0) {
//                my = pixelate(_mouseY - 1) + _c;
//            }
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

void LvkFrameDefWidget::paintFrameRect(QPainter& painter)
{
    QRect rect = _frect.normalized();

    if (!rect.isEmpty()) {
        if (mouseOverRect() != &_mrect) {
            paintResizeControls(painter, rect);
        }
        painter.setPen(_frectPen);
        painter.drawRect(rect);
    }
}

void LvkFrameDefWidget::paintMouseRect(QPainter& painter)
{
    QRect rect = _mrect.normalized();

    if (!rect.isEmpty()) {
        paintResizeControls(painter, rect);
        painter.setPen(_mrectPen);
        painter.drawRect(rect);
    }

}

void LvkFrameDefWidget::paintResizeControls(QPainter &painter, const QRect &rect)
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

void LvkFrameDefWidget::mousePressEvent(QMouseEvent *event)
{
    LvkInputImageWidget::mousePressEvent(event);

    _mouseClickX = event->x();
    _mouseClickY = event->y();

    if (event->buttons() & Qt::LeftButton) {
        mousePressLeftButtonEvent(event);
    } else if (event->buttons() & Qt::RightButton) {
        mousePressRightButtonEvent(event);
    }

    update();
}

void LvkFrameDefWidget::mousePressLeftButtonEvent(QMouseEvent */*event*/)
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
        _mrect.setRect(pixelate(_mouseClickX), pixelate(_mouseClickY), 0, 0);
        _resizingRect = ResizeBottomRight;
        emit mouseRectChangeFinished(ztor(_mrect));
    }

    if (_activeRect) {
        _mrectP = *_activeRect;
    } else {
        _mrectP = QRect();
    }
}

void LvkFrameDefWidget::mousePressRightButtonEvent(QMouseEvent */*event*/)
{
    if (mouseBlueGuideMode()) {
        _hGuide = !_hGuide;
//        } else if (_draggingRect) {
//            _mrect = _mrectP;
//            _draggingRect = false;
//            emit mouseRectChangeFinished(ztor(_mrect));
    } else {
        _mrect.setRect(0, 0, 0, 0);
        emit mouseRectChangeFinished(ztor(_mrect));
    }
}

void LvkFrameDefWidget::mouseMoveEvent(QMouseEvent *event)
{
    LvkInputImageWidget::mouseMoveEvent(event);

    if (event->buttons() & Qt::LeftButton) {
        mouseMoveUpdateRects();
    }

    update();
}

void LvkFrameDefWidget::mouseMoveUpdateRects()
{
    int dx = _mouseX - _mouseClickX;
    int dy = _mouseY - _mouseClickY;

    bool newMouseRect = false;

    if (!_activeRect) {
        newMouseRect = true;
        _activeRect = &_mrect;
    }

    QRect& rect = *_activeRect;

    if (_resizingRect) {
        if (_resizingRect & ResizeTop) {
            int y = _mrectP.y() + dy ;
            rect.setY(pixelate(y));
        }
        if (_resizingRect & ResizeRight) {
            int w = (newMouseRect) ? _mouseX - _mrect.x() : _mrectP.width() + dx;
            rect.setWidth(pixelate(w));
        }
        if (_resizingRect & ResizeBottom) {
            int h = (newMouseRect) ? _mouseY - _mrect.y() : _mrectP.height() + dy;
            rect.setHeight(pixelate(h));
        }
        if (_resizingRect & ResizeLeft) {
            int x = _mrectP.x() + dx ;
            rect.setX(pixelate(x));
        }
    } else if (_draggingRect) {
        if (!ctrlKey()) {
            rect.moveTo(pixelate(_mrectP.x() + dx), pixelate(_mrectP.y() + dy));
        } else {
            if (abs(dx) > 20) {
                rect.moveTo(pixelate(_mrectP.x() + dx), pixelate(_mrectP.y()));
            } else {
                rect.moveTo(pixelate(_mrectP.x()), pixelate(_mrectP.y() + dy));
            }
        }
    }

    emitRectChanging();
}

void LvkFrameDefWidget::mouseReleaseEvent(QMouseEvent *event)
{
    LvkInputImageWidget::mouseReleaseEvent(event);

    _mrect = _mrect.normalized();
    _frect = _frect.normalized();
    _draggingRect = false;
    _resizingRect = ResizeNull;

    emitRectChangeFinished();

    _activeRect = 0; /* Important: do this *after* calling emitRectChangeFinished */

    update();
}

void LvkFrameDefWidget::setMouseCursor()
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

void LvkFrameDefWidget::scrollToFrame(const LvkFrame& frame)
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

void LvkFrameDefWidget::emitRectChanging()
{
    if (_activeRect == &_mrect) {
        emit mouseRectChanging(ztor(_mrect));
    } else if (_activeRect == &_frect) {
        emit frameRectChanging(ztor(_frect));
    }
}

void LvkFrameDefWidget::emitRectChangeFinished()
{
    if (_activeRect == &_mrect) {
        emit mouseRectChangeFinished(ztor(_mrect));
    } else if (_activeRect == &_frect) {
        emit frameRectChangeFinished(ztor(_frect));
    }
}

LvkFrameDefWidget::~LvkFrameDefWidget()
{
}
