#include <QStringList>
#include <QDebug>

#include "lvkaframe.h"

LvkAframe::LvkAframe(Id id, Id frameId, int delay)
        : id(id), frameId(frameId), delay(delay)
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
    QString str("%1,%2,%3");

    return str.arg(QString::number(id),
                   QString::number(frameId),
                   QString::number(delay));
}

bool LvkAframe::fromString(const QString& str)
{
    QStringList list = str.split(",");

    if (list.size() == 3) {
        id      = list.at(0).toInt();
        frameId = list.at(1).toInt();
        delay   = list.at(2).toInt();
        return true;
    } else {
        qDebug() << "Warning LvkFrame::LvkAframe(const QString&) invalid string format";
        return false;
    }
}
