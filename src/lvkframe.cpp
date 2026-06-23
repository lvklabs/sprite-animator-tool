#include <QDebug>
#include <QStringList>

#include "lvkframe.h"

LvkFrame::LvkFrame(Id id, Id imgId, int ox, int oy, int w, int h, const QString &name)
    : id(id), imgId(imgId), ox(ox), oy(oy), w(w), h(h), name(name) {}

LvkFrame::LvkFrame(const QString &str) {
    if (!fromString(str)) {
        // TODO should throw an exception
    }
}

QString LvkFrame::toString() const {
    // Phase 6b (Item 26): see note in LvkAnimation::toString.  CSV format
    // cannot represent ',' or NUL in a name field; reject rather than
    // silently corrupt the saved file.
    if (name.contains(QLatin1Char(',')) || name.contains(QChar('\0'))) {
        qWarning() << "LvkFrame::toString refusing to serialize name containing comma or NUL:"
                   << name;
        return QString();
    }

    QString str("%1,%2,%3,%4,%5,%6,%7");

    return str.arg(QString::number(id), name, QString::number(imgId), QString::number(ox),
                   QString::number(oy), QString::number(w), QString::number(h));
}

bool LvkFrame::fromString(const QString &str) {
    QStringList list = str.split(",");

    if (list.size() == 7) {
        const int parsedW = list.at(5).toInt();
        const int parsedH = list.at(6).toInt();

        // SECURITY (Phase 4): Reject frame dimensions that would feed an
        // integer-overflow / OOM cascade in the atlas exporter
        // (4*w*h bytes for ARGB32) or in QPixmap::copy(). 8192 is the
        // largest texture dim shipping GPUs reliably support, so anything
        // above it is presumed malicious or corrupt.
        if (parsedW <= 0 || parsedH <= 0 || parsedW > 8192 || parsedH > 8192) {
            // Team B3 (Phase 6c): qWarning rather than qDebug so the
            // rejected frame name is visible to CLI users (release
            // builds strip qDebug). The record is dropped silently
            // otherwise.
            //
            // TODO (out of scope for B3): aggregate rejection count and
            // surface it to the GUI loader as a banner so users see
            // partial-load results.
            qWarning() << "LvkFrame::fromString: rejected out-of-range frame dimensions"
                       << "w=" << parsedW << "h=" << parsedH << "(name was:" << list.at(1) << ")";
            id = NullId;
            imgId = NullId;
            ox = oy = w = h = 0;
            name = QString();
            return false;
        }

        id = list.at(0).toInt();
        name = list.at(1);
        imgId = list.at(2).toInt();
        ox = list.at(3).toInt();
        oy = list.at(4).toInt();
        w = parsedW;
        h = parsedH;
        return true;
    } else {
        qWarning() << "LvkFrame::fromString: invalid string format:" << str;
        return false;
    }
}

QRect LvkFrame::rect() const {
    return QRect(ox, oy, w, h);
}

void LvkFrame::setRect(const QRect &rect) {
    ox = rect.x();
    oy = rect.y();
    w = rect.width();
    h = rect.height();
}
