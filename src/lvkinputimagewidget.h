#ifndef QINPUTIMAGEWIDGET_H
#define QINPUTIMAGEWIDGET_H

#include "lvkframe.h"
#include "types.h"
#include <QApplication>
#include <QBrush>
#include <QCache>
#include <QPair>
#include <QPixmap>
#include <QRect>
#include <QScrollArea>

class QPainter;

/// This widget is used to preview images, frames and a frames,
/// it provides zooming functions and contains an internal
/// cache to optimize the drawing of images.
class LvkInputImageWidget : public QWidget {
    Q_OBJECT

public:
    LvkInputImageWidget(QWidget *parent = 0);
    ~LvkInputImageWidget() override;

    static const int ZOOM_FACTOR = 2.0;
    static const int ZOOM_MIN = -3;
    static const int ZOOM_MAX = 3;

    // Agent 7: PCACHE_ROW_SIZE used to be the hard upper-bound size of a
    // raw QPixmap*[ROW][COL] table. Now backed by QCache, so this is just
    // the soft cap on the *number of distinct cache ids* the cache will
    // try to remember at once (callers still pass any non-negative Id —
    // it is no longer an array-index bound check).
    static const int PCACHE_ROW_SIZE = 1000;
    static const int PCACHE_COL_SIZE = ZOOM_MAX - ZOOM_MIN + 1;
    /// LRU capacity: cap memory at PCACHE_ROW_SIZE * PCACHE_COL_SIZE
    /// distinct (cacheId, zoomLevel) entries — matches the previous worst-
    /// case footprint of the raw array.
    static const int PCACHE_CAPACITY = PCACHE_ROW_SIZE * PCACHE_COL_SIZE;

    /// returns the current zoom level.
    int zoom();

    /// Set the current pixamp. If useCacheId is not null, then the
    /// widget uses an internal cache to increase the speed when drawing
    /// zoomed (scaled) images.
    /// @param useCacheId must be non-negative and unique for every
    /// different pixmap to get a cache hit. Values are no longer
    /// bounds-checked against PCACHE_ROW_SIZE — the underlying QCache
    /// accepts any Id, evicting cold entries when over capacity.
    void setPixmap(const QPixmap &pixmap, Id useCacheId = NullId);

    ///
    void setBlendPixmap(const QPixmap &pixmap);

    /// Clears the whole pixmap cache used to speed the drawing of zoomed images
    void clearPixmapCache();

    /// Clears a pixmap cache used to speed the drawing of zoomed images
    void clearPixmapCache(Id cacheId);

    /// Sets scroll parent widget
    void setScrollArea(QScrollArea *scroll);

    /// Sets the widget background
    void setBackground(const QPixmap &bg);

    /// Register rect to automatically scale when changing the zoom
    void registerRect(QRect *rect);

    /// Unregister rect
    void unregisterRect(QRect *rect);

public slots:
    void zoomIn();
    void zoomOut();
    void setZoom(int level);

    void clear();

signals:
    void mousePositionChanged(int x, int y);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    /// from real size to zoomed size (in pixels);
    inline int rtoz(int value) const { return value * _c; }

    /// @overload
    QRect rtoz(const QRect &rect) const;

    /// Convert from zoomed size to real size (in pixels);
    inline int ztor(int value) const { return value / _c; }

    /// @overload
    QRect ztor(const QRect &rect) const;

    /// pixelate value
    inline int pixelate(int value) const {
        int c = (int)_c;

        if (c == 0) {
            return value;
        } else if (value > 0) {
            value -= value % c;
            return (value < 0) ? 0 : value;
        } else if (value < 0) {
            value += (-1 * value) % c;
            return (value > 0) ? 0 : value + 1;
        } else {
            return 0;
        }
    }

    bool ctrlKey() const { return QApplication::keyboardModifiers() & Qt::ControlModifier; }

    bool shiftKey() const { return QApplication::keyboardModifiers() & Qt::ShiftModifier; }

    bool altKey() const { return QApplication::keyboardModifiers() & Qt::AltModifier; }

    float _c;             /* heavily used coeficient */
    int _mouseX;          /* mouse current x position */
    int _mouseY;          /* mouse current y position */
    int _zoom;            /* current zoom level */
    QScrollArea *_scroll; /* if the widget has a parent scroll */

private:
    QPixmap _pixmap;      /* current pixmap */
    QPixmap _blendPixmap; /* blend pixmap */
    QPixmap _bg;          /* background pixmap */
    QBrush _bgBrush;      /* background brush */
    Id _cacheId;          /* current cache */
    // Agent 7: was QPixmap* _pCache[1000][7] -- 7000 raw owning pointers
    // and a hand-coded delete[] loop. Replaced with QCache, which owns
    // the entries, evicts LRU-style when over PCACHE_CAPACITY, and
    // releases everything in its dtor. Key is (cacheId, zoomLevel).
    QCache<QPair<Id, int>, QPixmap> _pCache;
    QList<QRect *> _regRects; /* list of registered rects */

    void fillBackground(QPainter &painter, int x, int y, int w, int h);

    void updateRegRects(int level);

    void resize(const QSize &size);
    void resize(int w, int h);
};

#endif // QINPUTIMAGEWIDGET_H
