#ifndef QINPUTIMAGEWIDGET_H
#define QINPUTIMAGEWIDGET_H

#include <QPixmap>
#include <QRect>
#include <QBrush>
#include <QApplication>
#include <QScrollArea>
#include "types.h"
#include "lvkframe.h"

class QPainter;

/// This widget is used to preview images, frames and a frames,
/// it provides zooming functions and contains an internal
/// cache to optimize the drawing of images.
class LvkInputImageWidget : public QWidget
{
    Q_OBJECT

public:
    LvkInputImageWidget(QWidget *parent = 0);
    ~LvkInputImageWidget();

    static const int ZOOM_FACTOR = 2.0;
    static const int ZOOM_MIN    = -3;
    static const int ZOOM_MAX    = 3;

    static const int PCACHE_ROW_SIZE = 1000;
    static const int PCACHE_COL_SIZE = ZOOM_MAX - ZOOM_MIN + 1;

    /// returns the current zoom level.
    int zoom();

    /// Set the current pixamp. If useCacheId is not null, then the
    /// widget uses an internal cache to increase the speed when drawing
    /// zoomed (scaled) images.
    /// @param useCacheId must be greater than or equal to zero and less
    /// than PCACHE_ROW_SIZE and unique for every different pixmap.
    void setPixmap(const QPixmap &pixmap, Id useCacheId  = NullId);

    /// Clears the whole pixmap cache used to speed the drawing of zoomed images
    void clearPixmapCache();

    /// Clears a pixmap cache used to speed the drawing of zoomed images
    void clearPixmapCache(Id cacheId);

    /// Sets scroll parent widget
    // TODO this sucks, should be handled transparentely by the
    // LvkInputImageWidget class
    void setScrollArea(QScrollArea* scroll);

    /// Sets the widget background
    void setBackground(const QPixmap& bg);

public slots:
    void zoomIn();
    void zoomOut();
    void setZoom(int level);

    void clear();

signals:
    void mousePositionChanged(int x, int y);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);

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

    bool ctrlKey() const
    { return QApplication::keyboardModifiers() & Qt::ControlModifier; }

    bool shiftKey() const
    { return QApplication::keyboardModifiers() & Qt::ShiftModifier; }

    bool altKey() const
    { return QApplication::keyboardModifiers() & Qt::AltModifier; }

    //////////////////////////////////////////////////////////
    // FIXME these should live inside LvkFrameDefWidget
    QRect      _frect;         /* frame rect */
    QRect      _scaledFrect;   /* frame rect scaled by _zoom */
    QRect      _mouseRect;     /* mouse rect */
    //////////////////////////////////////////////////////////

    float        _c;             /* heavily used coeficient */
    int          _mouseX;        /* mouse current x position */
    int          _mouseY;        /* mouse current y position */
    int          _zoom;          /* current zoom level */
    QScrollArea* _scroll;        /* if the widget has a parent scroll */

private:

    QPixmap      _pixmap;        /* current pixmap */
    QPixmap      _bg;            /* background pixmap */
    QBrush       _bgBrush;       /* background brush */
    Id           _cacheId;       /* current cache */
    QPixmap*     _pCache[PCACHE_ROW_SIZE][PCACHE_COL_SIZE]; /* pixmap cache */

    void fillBackground(QPainter& painter, int x, int y, int w, int h);

    void resize(const QSize &size);
    void resize(int w, int h);
};

#endif // QINPUTIMAGEWIDGET_H
