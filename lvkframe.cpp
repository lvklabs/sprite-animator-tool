#include <QStringList>
#include <QDebug>

#include "lvkframe.h"

LvkFrame::LvkFrame(Id id, Id imgId, int ox, int oy, int w, int h, const QString& name)
        : id(id), imgId(imgId), ox(ox), oy(oy), w(w), h(h), name(name)
{
}

LvkFrame::LvkFrame(const QString& str)
{
    if (!fromString(str)) {
        // TODO should throw an exception
    }
}

QString LvkFrame::toString() const
{
    QString str("%1,%2,%3,%4,%5,%6,%7");

    return str.arg(QString::number(id),
                   name,
                   QString::number(imgId),
                   QString::number(ox),
                   QString::number(oy),
                   QString::number(w),
                   QString::number(h));
}

bool LvkFrame::fromString(const QString& str)
{
    QStringList list = str.split(",");

    if (list.size() == 7) {
        id    = list.at(0).toInt();
        name  = list.at(1);
        imgId = list.at(2).toInt();
        ox    = list.at(3).toInt();
        oy    = list.at(4).toInt();
        w     = list.at(5).toInt();
        h     = list.at(6).toInt();
        return true;
    } else {
        qDebug() << "Warning LvkFrame::LvkFrame(const QString&) invalid string format";
        return false;
    }
}

QRect LvkFrame::rect() const
{
    return QRect(ox, oy, w, h);
}

void LvkFrame::setRect(const QRect &rect)
{
    ox = rect.x();
    oy = rect.y();
    w = rect.width();
    h = rect.height();
}

