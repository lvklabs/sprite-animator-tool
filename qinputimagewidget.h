#ifndef QINPUTIMAGEWIDGET_H
#define QINPUTIMAGEWIDGET_H

#include <QLabel>
#include <QRect>

class QInputImageWidget : public QLabel
{
    Q_OBJECT

public:
    QInputImageWidget(QWidget *parent = 0);

    void setPixmap(const QPixmap &pixmap);

public slots:
    void zoomIn();
    void zoomOut();

    void setFrameRectVisible(bool visible);
    bool frameRectVisible() const;

    void setFrameRect(const QRect &rect);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);

private:
    static const int ZOOM_FACTOR = 2;
    static const int ZOOM_MIN    = 0;
    static const int ZOOM_MAX    = 4;

    QRect   _rect;
    QRect   _scaledRect;
    bool    _rectVisible;
    int     _zoom;
    QPixmap _pixmap;

    void updateScaledRect();
    void resize(const QSize &size);
    void resize(int w, int h);
};

#endif // QINPUTIMAGEWIDGET_H
