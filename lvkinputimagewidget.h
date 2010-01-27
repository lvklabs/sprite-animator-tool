#ifndef QINPUTIMAGEWIDGET_H
#define QINPUTIMAGEWIDGET_H

#include <QLabel>
#include <QRect>
#include <QPen>
#include <QApplication>

class LvkInputImageWidget : public QWidget
{
    Q_OBJECT

public:
    LvkInputImageWidget(QWidget *parent = 0);
    ~LvkInputImageWidget();

    void setPixmap(const QPixmap &pixmap);

    static const int ZOOM_FACTOR = 2.0;
    static const int ZOOM_MIN    = -3;
    static const int ZOOM_MAX    = 3;

    int zoom();
    const QRect frameRect() const;
    const QRect mouseFrameRect() const;

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
    void mouseRectChanged(const QRect& rect);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);

private:
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

    float    _c;             /* heavily used coeficient */
    QRect    _frect;         /* frame rect */
    QRect    _scaledFrect;   /* frame rect scaled by _zoom */
    QRect    _mouseRect;     /* mouse rect */
    QRect    _mouseRectP;    /* mouse rect used for dragging */
    int      _mouseX;        /* mouse current x position */
    int      _mouseY;        /* mouse current y position */
    int      _mouseClickX;   /* mouse click x position */
    int      _mouseClickY;   /* mouse click y position */
    bool     _frectVisible;  /* turn on/off visible rects */
    bool     _mouseLinesVisible; /* turn on/off mouse lines */
    int      _zoom;          /* current zoom level */
    QPixmap  _pixmap;        /* current pixmap */
    QPen     _frectPen;      /* pen used to draw the frame rect */
    QPen     _mouseRectPen;  /* pen used to draw the mouse rect */
    QPen     _mouseGuidePen; /* pen used to draw the mouse guides */
    QPen     _guidePen;      /* pen used to draw the "blue" guides */
    bool     _draggingRect;  /* if dragging the mouse rect */
    bool     _resizingRect;  /* if resizing the mouse rect */
    bool     _hGuide;        /* add horizontal guide if true, add vertical guide if false */

    QList<QPoint> _guides;

    bool ctrlKey()
    { return QApplication::keyboardModifiers() & Qt::ControlModifier; }

    bool shiftKey()
    { return QApplication::keyboardModifiers() & Qt::ShiftModifier; }

    bool altKey()
    { return QApplication::keyboardModifiers() & Qt::AltModifier; }

    bool mouseCrossGuidesMode();
    bool mouseBlueGuideMode();
    bool canDrag();

    void resize(const QSize &size);
    void resize(int w, int h);
};

#endif // QINPUTIMAGEWIDGET_H
