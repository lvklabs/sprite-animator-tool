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
        const int parsedW = list.at(5).toInt();
        const int parsedH = list.at(6).toInt();

        // SECURITY (Phase 4): Reject frame dimensions that would feed an
        // integer-overflow / OOM cascade in the atlas exporter
        // (4*w*h bytes for ARGB32) or in QPixmap::copy(). 8192 is the
        // largest texture dim shipping GPUs reliably support, so anything
        // above it is presumed malicious or corrupt.
        if (parsedW <= 0 || parsedH <= 0 || parsedW > 8192 || parsedH > 8192) {
            qDebug() << "Warning LvkFrame::fromString rejected out-of-range frame dimensions"
                     << "w=" << parsedW << " h=" << parsedH;
            id    = NullId;
            imgId = NullId;
            ox = oy = w = h = 0;
            name = QString();
            return false;
        }

        id    = list.at(0).toInt();
        name  = list.at(1);
        imgId = list.at(2).toInt();
        ox    = list.at(3).toInt();
        oy    = list.at(4).toInt();
        w     = parsedW;
        h     = parsedH;
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

