#ifndef LVKFRAMEGRAPHICSGROUP_H
#define LVKFRAMEGRAPHICSGROUP_H

#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QTimerEvent>
#include <QPoint>

#include "lvkanimation.h"


class LvkAnimationWidget : public QWidget
{
public:
    LvkAnimationWidget(QWidget* parent = 0);

    void setAnimation(const LvkAnimation& ani, const QMap<Id, QPixmap>& fpixmaps);
    void setAnimations(const QList<LvkAnimation>& anis, const QMap<Id, QPixmap>& fpixmaps);
    void setScreenSize(int w, int h);
    void play();
    void stop();
    bool isPlaying();
    void clear();

protected:
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    LvkAnimationWidget(const LvkAnimationWidget&);
    LvkAnimationWidget& operator=(const LvkAnimationWidget&);

    QList<QPixmap> _fpixmaps;
    QList<int> _delays;
    QList<int> _oxs;
    QList<int> _oys;
    QList<bool> _stickies;

    int    _currentFrame;
    int    _currentTimer;
    bool   _isPlaying;
    int    _scrW;
    int    _scrH;
    QPoint _origin;

    void nextFrame();
};

#endif // LVKFRAMEGRAPHICSGROUP_H
