#ifndef QINPUTIMAGEWIDGET_H
#define QINPUTIMAGEWIDGET_H

#include <QLabel>
#include <QRect>
#include <QPen>
#include <QApplication>
#include <QScrollArea>
#include "types.h"
#include "lvkframe.h"

class QPainter;

class LvkInputImageWidget : public QWidget
{
    Q_OBJECT

public:
    LvkInputImageWidget(QWidget *parent = 0);
    ~LvkInputImageWidget();

    static const int ZOOM_FACTOR = 2.0;
    static const int ZOOM_MIN    = -3;
    static const int ZOOM_MAX    = 3;

    /// returns the current zoom level.
    int zoom();

    static const int PCACHE_ROW_SIZE = 1000;
    static const int PCACHE_COL_SIZE = ZOOM_MAX - ZOOM_MIN + 1;

    /// Set the current pixamp. If useCacheId is not null, then the
    /// widget uses an internal cache to increase the speed when drawing
    /// zoomed (scaled) images.
    /// @param useCacheId must be greater than or equal to zero and less
    /// than PCACHE_ROW_SIZE and unique for every different pixmap.
    void setPixmap(const QPixmap &pixmap, Id useCacheId  = NullId);

    /// Clear the pixmap cache used to speed the drawing of zoomed images
    void clearPixmapCache();

    const QRect frameRect() const;
    const QRect mouseFrameRect() const;

    /// Sets scroll parent widget
    // TODO this sucks, should be handled transparentely by the
    // LvkInputImageWidget class
    void setScrollArea(QScrollArea* scroll);

    /// Scroll widget to show a frame.
    /// setScrollArea() must be called before invoking this method.
    void scrollToFrame(const LvkFrame& frame);

    /// Sets the widget background
    void setBackground(const QPixmap& bg);

public slots:
    void zoomIn();
    void zoomOut();
    void setZoom(int level);

    void setFrameRectVisible(bool visible);
    bool frameRectVisible() const;

    void setMouseLinesVisible(bool visible);
    bool mouseLinesRectVisible() const;

    void setFrameRect(const QRect &rect);

    void clear();
    void clearGuides();

signals:
    void mousePositionChanged(int x, int y);
    void mouseRectChanging(const QRect& rect);
    void frameRectChanging(const QRect& rect);
    void mouseRectChangeFinished(const QRect& rect);
    void frameRectChangeFinished(const QRect& rect);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);

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

    /// from real size to zoomed size (in pixels);
    inline int rtoz(int value)  const
    { return value*_c; }

    /// @overload
    QRect rtoz(const QRect& rect) const;

    /// Convert from zoomed size to real size (in pixels);
    inline int ztor(int value) const
    { return value/_c; }

    /// @overload
    QRect ztor(const QRect& rect) const;

    /// pixelate value
    inline int pixelate(int value) const
    {
        int c = (int)_c;

        if (c == 0) {
            return value;
        } else if (value > 0) {
            value -=  value % c;
            return (value < 0) ? 0 : value;
        } else if (value < 0) {
            value +=  (-1*value) % c;
            return (value > 0) ? 0 : value + 1;
        } else {
            return 0;
        }
    }

    float      _c;             /* heavily used coeficient */
    QRect      _frect;         /* frame rect */
    QRect      _scaledFrect;   /* frame rect scaled by   _zoom */
    QRect      _mouseRect;     /* mouse rect */
    QRect      _mouseRectP;    /* mouse rect used for dragging */
    QRect*     _activeRect;    /* active (resizing or dragging) rect */
    int        _mouseX;        /* mouse current x position */
    int        _mouseY;        /* mouse current y position */
    int        _mouseClickX;   /* mouse click x position */
    int        _mouseClickY;   /* mouse click y position */
    bool       _frectVisible;  /* turn on/off visible rects */
    bool       _mouseLinesVisible; /* turn on/off mouse lines */
    int        _zoom;          /* current zoom level */
    QPixmap    _pixmap;        /* current pixmap */
    QPixmap     _bg;            /* widget background */
    QBrush     _bgBrush;       /* background brush */
    QPen       _frectPen;      /* pen used to draw the frame rect */
    QPen       _mouseRectPen;  /* pen used to draw the mouse rect */
    QPen       _mouseGuidePen; /* pen used to draw the mouse guides */
    QPen       _guidePen;      /* pen used to draw the "blue" guides */
    QPen       _resizeControlsPen; /* pen used to draw the resize controls */
    bool       _draggingRect;  /* if dragging the mouse rect */
    ResizeType _resizingRect;  /* if resizing the new mouse rect */
    bool       _hGuide;        /* add horizontal guide if true, add vertical guide if false */
    Id         _cacheId;       /* */

    QList<QPoint> _guides;   /* list of "blue" guides */
    QScrollArea*  _scroll;   /* if the widget has a parent scroll */

    QPixmap*      _pCache[PCACHE_ROW_SIZE][PCACHE_COL_SIZE]; /* pixmap cache */

    bool ctrlKey() const
    { return QApplication::keyboardModifiers() & Qt::ControlModifier; }

    bool shiftKey() const
    { return QApplication::keyboardModifiers() & Qt::ShiftModifier; }

    bool altKey() const
    { return QApplication::keyboardModifiers() & Qt::AltModifier; }

    inline bool mouseCrossGuidesMode() const;
    inline bool mouseBlueGuideMode() const;

    static const int RESIZE_CONTROL_SIZE = 20;

    inline bool isMouseOver(const QRect& rect)  const;
    bool isMouseOverResizeControls(const QRect& rect) const;
    const QRect* mouseOverRect(bool withResizeControls = true) const;

    bool canDrag() const;
    bool canResize(ResizeType type) const;

    void setMouseCursor();

    void paintImage(QPainter& painter);
    void paintGuides(QPainter& painter);
    void paintMouseGuides(QPainter& painter);
    void paintFrameRect(QPainter& painter);
    void paintMouseRect(QPainter& painter);
    void paintResizeControls(QPainter& painter, const QRect& rect);

    void mousePressLeftButtonEvent(QMouseEvent *event);
    void mousePressRightButtonEvent(QMouseEvent *event);
    void mouseMoveUpdateRects();

    void fillBackground(QPainter& painter, int x, int y, int w, int h);

    void resize(const QSize &size);
    void resize(int w, int h);

    void emitRectChanging();
    void emitRectChangeFinished();
};

#endif // QINPUTIMAGEWIDGET_H
