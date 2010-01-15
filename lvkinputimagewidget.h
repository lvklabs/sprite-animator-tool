#ifndef QINPUTIMAGEWIDGET_H
#define QINPUTIMAGEWIDGET_H

#include <QLabel>
#include <QRect>
#include <QPen>

class LvkInputImageWidget : public QWidget
{
    Q_OBJECT

public:
    LvkInputImageWidget(QWidget *parent = 0);
    ~LvkInputImageWidget();

    void setPixmap(const QPixmap &pixmap);

public slots:
    void zoomIn();
    void zoomOut();

    void setFrameRectVisible(bool visible);
    bool frameRectVisible() const;

    void setMouseLinesVisible(bool visible);
    bool mouseLinesRectVisible() const;

    void setFrameRect(const QRect &rect);
    const QRect frameRect() const;
    const QRect mouseFrameRect() const;

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
    static const int ZOOM_FACTOR = 2;
    static const int ZOOM_MIN    = 0; /* Do not change! */
    static const int ZOOM_MAX    = 3;

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
        if (value > 0) {
            value -=  value % _c;
            return (value < 0) ? 0 : value;
        } else if (value < 0) {
            value +=  (-1*value) % _c;
            return (value > 0) ? 0 : value + 1;
        }
        return 0;
    }

    int      _c;             /* heavily used coeficient */
    QRect    _frect;         /* frame rect */
    QRect    _scaledFrect;   /* frame rect scaled by _zoom */
    QRect    _mouseRect;     /* mouse rect */
    int      _mouseX;        /* mouse current x position */
    int      _mouseY;        /* mouse current y position */
    bool     _frectVisible;  /* turn on/off visible rects */
    bool     _mouseLinesVisible; /* turn on/off mouse lines */
    int      _zoom;          /* current zoom level */
    QPixmap  _pixmap;        /* current pixmap */
    QPen     _frectPen;      /* pen used to draw the frame rect */
    QPen     _mouseRectPen;  /* pen used to draw the mouse rect */

    void resize(const QSize &size);
    void resize(int w, int h);
};

#endif // QINPUTIMAGEWIDGET_H
