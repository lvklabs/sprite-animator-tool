#ifndef QLVKFRAEMDEFWIDGET_H
#define QLVKFRAEMDEFWIDGET_H

#include <QLabel>
#include <QRect>
#include <QPen>
#include <QScrollArea>
#include "types.h"
#include "lvkframe.h"
#include "lvkinputimagewidget.h"

class QPainter;

/// Frame Definition Widget. As the name indicates this widget
/// is used to define frames.
class LvkFrameDefWidget : public LvkInputImageWidget
{
    Q_OBJECT

public:
    LvkFrameDefWidget(QWidget *parent = 0);
    virtual ~LvkFrameDefWidget();

    /// Return current frame rect
    const QRect frameRect() const;

    /// Return mouse rect
    const QRect mouseFrameRect() const;

    /// Scroll widget to show a frame.
    /// setScrollArea() must be called before invoking this method.
    void scrollToFrame(const LvkFrame& frame);

public slots:

    /// Set and display a frame rect
    void setFrameRect(const QRect &rect);

    /// clear widget
    void clear();

    /// clear line guides
    void clearGuides();

signals:
    void mouseRectChanging(const QRect& rect);
    void frameRectChanging(const QRect& rect);
    void mouseRectChangeFinished(const QRect& rect);
    void frameRectChangeFinished(const QRect& rect);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:

    enum ResizeType {
        ResizeNull        = 0x00,
        ResizeTop         = 0x01,
        ResizeRight       = 0x02,
        ResizeBottom      = 0x04,
        ResizeLeft        = 0x08,
        ResizeTopRight    = ResizeTop | ResizeRight,
        ResizeTopLeft     = ResizeTop | ResizeLeft,
        ResizeBottomRight = ResizeBottom | ResizeRight,
        ResizeBottomLeft  = ResizeBottom | ResizeLeft,
    };

    QRect      _frect;         /* frame rect (i.e. the frame that we are currently displaying) */
    QRect      _mrect;         /* mouse rect (i.e. rect defined with mouse) */
    QRect      _mrectP;        /* mouse rect used for dragging */
    QRect*     _activeRect;    /* active (resizing or dragging) rect */
    int        _mouseClickX;   /* mouse click x position */
    int        _mouseClickY;   /* mouse click y position */
    QPen       _frectPen;      /* pen used to draw the frame rect */
    QPen       _mrectPen;      /* pen used to draw the mouse rect */
    QPen       _mouseGuidePen; /* pen used to draw the mouse guides */
    QPen       _guidePen;      /* pen used to draw the "blue" guides */
    QPen       _resizeControlsPen; /* pen used to draw the resize controls */
    bool       _draggingRect;  /* if dragging the mouse rect */
    ResizeType _resizingRect;  /* if resizing the new mouse rect */
    bool       _hGuide;        /* add horizontal guide if true, add vertical guide if false */

    QList<QPoint> _guides;   /* list of "blue" guides */

    inline bool mouseCrossGuidesMode() const;
    inline bool mouseBlueGuideMode() const;

    static const int RESIZE_CONTROL_SIZE = 20;

    inline bool isMouseOver(const QRect& rect)  const;
    bool isMouseOverResizeControls(const QRect& rect) const;
    const QRect* mouseOverRect(bool withResizeControls = true) const;

    bool canDrag() const;
    bool canResize(ResizeType type) const;

    void setMouseCursor();

    void paintGuides(QPainter& painter);
    void paintMouseGuides(QPainter& painter);
    void paintFrameRect(QPainter& painter);
    void paintMouseRect(QPainter& painter);
    void paintResizeControls(QPainter& painter, const QRect& rect);

    void mousePressLeftButtonEvent(QMouseEvent *event);
    void mousePressRightButtonEvent(QMouseEvent *event);
    void mouseMoveUpdateRects();

    void emitRectChanging();
    void emitRectChangeFinished();
};

#endif // QLVKFRAEMDEFWIDGET_H
