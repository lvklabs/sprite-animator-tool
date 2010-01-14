#ifndef LVKFRAMEGRAPHICSGROUP_H
#define LVKFRAMEGRAPHICSGROUP_H

#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QTimerEvent>

#include "lvkanimation.h"


class LvkAnimationWidget : public QWidget
{
public:
    LvkAnimationWidget(QWidget* parent = 0,
                       const LvkAnimation& ani = LvkAnimation(),
                       const QHash<Id, QPixmap>& fpixmaps = QHash<Id, QPixmap>());

    void setAnimation(const LvkAnimation& ani, const QHash<Id, QPixmap>& fpixmaps);
    void play();
    void stop();
    bool isPlaying();
    void clear();

protected:
    virtual void timerEvent(QTimerEvent* event);
    virtual void paintEvent(QPaintEvent *event);

private:
    LvkAnimationWidget(const LvkAnimationWidget&);
    LvkAnimationWidget& operator=(const LvkAnimationWidget&);

    QList<QPixmap> _fpixmaps;
    QList<int> _delays;

    int  _currentFrame;
    int  _currentTimer;
    bool _isPlaying;

    void nextFrame();
};

#endif // LVKFRAMEGRAPHICSGROUP_H
