#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <cmath>
#include <memory>

#include "lvkinputimagewidget.h"

QRect operator*(const QRect &rect, int c) {
    QRect tmp;
    tmp.setX(rect.x() * c);
    tmp.setY(rect.y() * c);
    tmp.setWidth(rect.width() * c);
    tmp.setHeight(rect.height() * c);
    return tmp;
}

QRect operator/(const QRect &rect, int c) {
    QRect tmp;
    tmp.setX(rect.x() / c);
    tmp.setY(rect.y() / c);
    tmp.setWidth(rect.width() / c);
    tmp.setHeight(rect.height() / c);
    return tmp;
}

LvkInputImageWidget::LvkInputImageWidget(QWidget *parent)
    : QWidget(parent), _mouseX(-1), _mouseY(-1), _zoom(0), _scroll(0), _cacheId(NullId),
      _pCache(PCACHE_CAPACITY) {
    _c = pow(ZOOM_FACTOR, _zoom);
    setMouseTracking(true);
}

void LvkInputImageWidget::clear() {
    _pixmap = QPixmap();
    _zoom = 0;
    _c = pow(ZOOM_FACTOR, _zoom);
    _blendPixmap = QPixmap();
    clearPixmapCache();
}

void LvkInputImageWidget::clearPixmapCache() {
    _pCache.clear();
    _cacheId = NullId;
}

void LvkInputImageWidget::clearPixmapCache(Id cacheId) {
    // Drop every entry whose key.first matches cacheId. We iterate the
    // small set of zoom levels rather than scanning the whole cache.
    for (int z = 0; z < PCACHE_COL_SIZE; ++z) {
        _pCache.remove(qMakePair(cacheId, z));
    }
}

void LvkInputImageWidget::setPixmap(const QPixmap &pixmap, Id useCacheId) {
    if (useCacheId != NullId) {
        if (useCacheId < 0) {
            qDebug() << "WARNING: LvkInputImageWidget::setPixmap() useCacheId negative, using 0";
            useCacheId = 0;
        }

        // Team H3: if the caller reuses a cache id for a *different*
        // pixmap (the common case is image reload after disk edit), the
        // entries scaled from the previous pixmap are now stale and
        // must be evicted -- otherwise paintEvent serves the old scaled
        // copy and the user sees no visible refresh. QPixmap::cacheKey()
        // is the canonical "same underlying data" probe and is cheap.
        // We use it to compare the new pixmap against whatever was
        // stored for this id, falling back to clearing on a fresh id
        // (when _cacheId did not previously map to this useCacheId).
        if (useCacheId != _cacheId || _pixmap.cacheKey() != pixmap.cacheKey()) {
            clearPixmapCache(useCacheId);
        }
        _cacheId = useCacheId;
    }

    _pixmap = pixmap;

    resize(_pixmap.width() * _c + 1, _pixmap.height() * _c + 1);
}

void LvkInputImageWidget::setBlendPixmap(const QPixmap &pixmap) {
    _blendPixmap = pixmap;
}

void LvkInputImageWidget::setBackground(const QPixmap &bg) {
    _bg = bg;
    _bgBrush = QBrush(bg);
}

void LvkInputImageWidget::registerRect(QRect *rect) {
    _regRects.append(rect);
}

void LvkInputImageWidget::unregisterRect(QRect *rect) {
    _regRects.removeOne(rect);
}

void LvkInputImageWidget::updateRegRects(int level) {
    int zoom = _zoom;

    if (zoom < level) {
        do {
            for (int i = 0; i < _regRects.size(); ++i) {
                *_regRects.at(i) = *_regRects.at(i) * 2;
            }
        } while (++zoom < level);
    } else if (zoom > level) {
        do {
            for (int i = 0; i < _regRects.size(); ++i) {
                *_regRects.at(i) = *_regRects.at(i) / 2;
            }
        } while (--zoom > level);
    }
}

#define SET_ZOOM(level)                                                                            \
    updateRegRects(level);                                                                         \
    _zoom = (level);                                                                               \
    _c = pow(ZOOM_FACTOR, _zoom);                                                                  \
    resize(_pixmap.width() * _c + 1, _pixmap.height() * _c + 1);

void LvkInputImageWidget::zoomIn() {
    if (_zoom < ZOOM_MAX) {
        SET_ZOOM(_zoom + 1);
    }
}

void LvkInputImageWidget::zoomOut() {
    if (_zoom > ZOOM_MIN) {
        SET_ZOOM(_zoom - 1);
    }
}

void LvkInputImageWidget::setZoom(int level) {
    if (level >= ZOOM_MIN && level <= ZOOM_MAX && level != _zoom) {
        SET_ZOOM(level);
    }
}

int LvkInputImageWidget::zoom() {
    return _zoom;
}

QRect LvkInputImageWidget::ztor(const QRect &rect) const {
    QRect tmp;
    tmp.setX(ztor(rect.x()));
    tmp.setY(ztor(rect.y()));
    tmp.setWidth(ztor(rect.width()));
    tmp.setHeight(ztor(rect.height()));
    return tmp;
}

QRect LvkInputImageWidget::rtoz(const QRect &rect) const {
    QRect tmp;
    tmp.setX(rtoz(rect.x()));
    tmp.setY(rtoz(rect.y()));
    tmp.setWidth(rtoz(rect.width()));
    tmp.setHeight(rtoz(rect.height()));
    return tmp;
}

void LvkInputImageWidget::setScrollArea(QScrollArea *scroll) {
    _scroll = scroll;
}

void LvkInputImageWidget::fillBackground(QPainter &painter, int x, int y, int w, int h) {
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

void LvkInputImageWidget::paintEvent(QPaintEvent * /*event*/) {
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
            painter.drawPixmap(0, 0, _blendPixmap.width() * _c, _blendPixmap.height() * _c,
                               _blendPixmap);
        }

        if (_cacheId != NullId) {
            const auto key = qMakePair(_cacheId, z);
            QPixmap *cached = _pCache.object(key);
            if (!cached) {
                // QCache takes ownership; if insertion fails (cost > cache
                // capacity) it deletes the pixmap immediately, returns
                // false, and object() will be null next look-up — we just
                // fall through to the un-cached scaled draw below.
                auto fresh = std::make_unique<QPixmap>(
                    _pixmap.scaled(_pixmap.width() * _c, _pixmap.height() * _c));
                QPixmap *raw = fresh.get();
                if (_pCache.insert(key, fresh.release())) {
                    cached = _pCache.object(key);
                    Q_UNUSED(raw);
                } else {
                    // Insert failed — `raw` was already deleted by QCache.
                    // Fall back to a single-shot scaled draw.
                    painter.drawPixmap(hval, vval, w, h,
                                       _pixmap.scaled(_pixmap.width() * _c, _pixmap.height() * _c),
                                       hval, vval, w, h);
                }
            }
            if (cached) {
                painter.drawPixmap(hval, vval, w, h, *cached, hval, vval, w, h);
            }
        } else {
            painter.drawPixmap(hval, vval, w, h,
                               _pixmap.scaled(_pixmap.width() * _c, _pixmap.height() * _c), hval,
                               vval, w, h);
        }
    } else {
        if (!_blendPixmap.isNull()) {
            painter.setOpacity(0.5);
            painter.drawPixmap(0, 0, _blendPixmap.width() * _c, _blendPixmap.height() * _c,
                               _blendPixmap);
        }

        painter.drawPixmap(0, 0, _pixmap.width() * _c, _pixmap.height() * _c, _pixmap);
    }
}

void LvkInputImageWidget::mousePressEvent(QMouseEvent * /*event*/) {}

void LvkInputImageWidget::mouseReleaseEvent(QMouseEvent * /*event*/) {}

void LvkInputImageWidget::mouseMoveEvent(QMouseEvent *event) {
    // Qt6: QMouseEvent::x()/y() are deprecated; use position() (returns QPointF).
    const QPoint p = event->position().toPoint();
    _mouseX = p.x();
    _mouseY = p.y();

    emit mousePositionChanged(ztor(_mouseX), ztor(_mouseY));
}

void LvkInputImageWidget::wheelEvent(QWheelEvent *event) {
    // Team H3: previously the function called event->accept() inside the
    // Ctrl-zoom branch and then an UNCONDITIONAL event->ignore() at the
    // tail, so Qt's "last call wins" semantics for accept/ignore meant
    // the parent QScrollArea always received the wheel event too -- the
    // image zoomed AND the scrollbar moved on every Ctrl+Wheel tick.
    // The else branch keeps the legacy "no modifier ==> let the scroll
    // area scroll" behaviour intact.
    if (ctrlKey()) {
        const int dy = event->angleDelta().y();
        if (dy > 0) {
            zoomIn();
        } else if (dy < 0) {
            zoomOut();
        }
        event->accept();
    } else {
        event->ignore();
    }
}

void LvkInputImageWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_1) {
        setZoom(0);
    }
}

void LvkInputImageWidget::resize(const QSize &size) {
    QWidget::resize(size);
    updateGeometry();
    update();
}

void LvkInputImageWidget::resize(int w, int h) {
    resize(QSize(w, h));
}

LvkInputImageWidget::~LvkInputImageWidget() {
    // QCache owns its entries and clears them on destruction; no manual
    // delete loop needed.
}
