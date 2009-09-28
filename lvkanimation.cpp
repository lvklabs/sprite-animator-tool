#include <QStringList>
#include <QDebug>

#include "lvkanimation.h"

LvkAnimation::LvkAnimation(Id id, const QString& name)
        : id(id), name(name)
{
}

LvkAnimation::LvkAnimation(const QString& str)
{
    if (!fromString(str)) {
        // TODO should throw an exception
    }
}

QString LvkAnimation::toString() const
{
    QString str("%1,%2");

    return str.arg(QString::number(id), name);
}

bool LvkAnimation::fromString(const QString& str)
{
    QStringList list = str.split(",");

    if (list.size() == 2) {
        id   = list.at(0).toInt();
        name = list.at(1);
        return true;
    } else {
        qDebug() << "Warning LvkAnimation::fromString(const QString&) invalid string format";
        return false;
    }
}
