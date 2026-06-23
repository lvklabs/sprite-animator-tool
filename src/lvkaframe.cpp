#include <QDebug>
#include <QStringList>

#include "lvkaframe.h"
#include "spritestate.h"

LvkAframe::LvkAframe(Id id, Id frameId, int delay, int ox, int oy, bool sticky)
    : id(id), frameId(frameId), delay(delay), ox(ox), oy(oy), sticky(sticky) {}

LvkAframe::LvkAframe(const QString &str) {
    if (!fromString(str)) {
        // TODO should throw an exception
    }
}

QString LvkAframe::toString() const {
    return toString(LvkVersion::V_04);
}

QString LvkAframe::toString(LvkVersion v) const {
    // v0.1: only the 3 original columns. v0.2/v0.3 added ox,oy. v0.4
    // added the sticky flag.
    if (v < LvkVersion::V_02) {
        return QStringLiteral("%1,%2,%3")
            .arg(QString::number(id), QString::number(frameId), QString::number(delay));
    }
    if (v < LvkVersion::V_04) {
        return QStringLiteral("%1,%2,%3,%4,%5")
            .arg(QString::number(id), QString::number(frameId), QString::number(delay),
                 QString::number(ox), QString::number(oy));
    }
    return QStringLiteral("%1,%2,%3,%4,%5,%6")
        .arg(QString::number(id), QString::number(frameId), QString::number(delay),
             QString::number(ox), QString::number(oy), QString::number(sticky));
}

bool LvkAframe::fromString(const QString &str) {
    QStringList list = str.split(",");

    if (list.size() == 6) {
        id = list.at(0).toInt();
        frameId = list.at(1).toInt();
        delay = list.at(2).toInt();
        ox = list.at(3).toInt();
        oy = list.at(4).toInt();
        sticky = list.at(5).toInt();
        return true;
    } else if (list.size() == 5) { /* backward compatibility */
        id = list.at(0).toInt();
        frameId = list.at(1).toInt();
        delay = list.at(2).toInt();
        ox = list.at(3).toInt();
        oy = list.at(4).toInt();
        sticky = false;
        return true;
    } else if (list.size() == 3) { /* backward compatibility */
        id = list.at(0).toInt();
        frameId = list.at(1).toInt();
        delay = list.at(2).toInt();
        ox = 0;
        oy = 0;
        sticky = false;
        return true;
    } else {
        qDebug() << "Warning LvkFrame::LvkAframe(const QString&) invalid string format";
        return false;
    }
}
