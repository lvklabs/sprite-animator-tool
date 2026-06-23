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
    // Phase B1.1: The aframe on-disk format actually has THREE valid
    // shapes that the parser has always accepted:
    //   3-field (id,frameId,delay)            -- the original v0.1 record
    //   5-field (id,frameId,delay,ox,oy)      -- ox/oy were always
    //                                            accepted under any
    //                                            header (per the parser
    //                                            in fromString below),
    //                                            and mario.lvks ships as
    //                                            v0.1 with 5-field
    //                                            records carrying
    //                                            nonzero ox/oy.
    //   6-field (id,frameId,delay,ox,oy,sticky) -- v0.4 only.
    //
    // The legacy gate "v0.1 -> 3-field only" silently DROPPED ox/oy on
    // any save of a v0.1 file with nonzero offsets -- exactly the
    // headline data-loss regression. Emit the 5-field form whenever
    // ox/oy is nonzero, regardless of version: the v0.1/v0.2/v0.3
    // readers all accept it (see fromString's branches below), and v0.4
    // adds the sixth column for sticky.
    if (v < LvkVersion::V_04) {
        if (ox != 0 || oy != 0) {
            return QStringLiteral("%1,%2,%3,%4,%5")
                .arg(QString::number(id), QString::number(frameId), QString::number(delay),
                     QString::number(ox), QString::number(oy));
        }
        // No ox/oy data -> emit the minimal 3-field form. This keeps
        // truly pristine v0.1 files (writePristineV01 in the
        // version-preservation test, for example) byte-equivalent
        // through round-trip.
        return QStringLiteral("%1,%2,%3")
            .arg(QString::number(id), QString::number(frameId), QString::number(delay));
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
