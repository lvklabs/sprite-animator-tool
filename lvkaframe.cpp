#include <QStringList>
#include <QDebug>

#include "lvkaframe.h"

LvkAframe::LvkAframe(Id id, Id frameId, int delay, int ox, int oy)
        : id(id), frameId(frameId), delay(delay), ox(ox), oy(oy)
{
}

LvkAframe::LvkAframe(const QString& str)
{
    if (!fromString(str)) {
        // TODO should throw an exception
    }
}

QString LvkAframe::toString() const
{
    QString str("%1,%2,%3,%4,%5");

    return str.arg(QString::number(id),
                   QString::number(frameId),
                   QString::number(delay),
                   QString::number(ox),
                   QString::number(oy));
}

bool LvkAframe::fromString(const QString& str)
{
    QStringList list = str.split(",");

    if (list.size() == 5) {
        id      = list.at(0).toInt();
        frameId = list.at(1).toInt();
        delay   = list.at(2).toInt();
        ox      = list.at(3).toInt();
        oy      = list.at(4).toInt();
        return true;
    } else if (list.size() == 3) { /* backward compatibility */
        id      = list.at(0).toInt();
        frameId = list.at(1).toInt();
        delay   = list.at(2).toInt();
        ox      = 0;
        oy      = 0;
        return true;
    } else {
        qDebug() << "Warning LvkFrame::LvkAframe(const QString&) invalid string format";
        return false;
    }
}
