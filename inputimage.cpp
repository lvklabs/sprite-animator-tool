#include <QStringList>
#include <QDebug>

#include "inputimage.h"

InputImage::InputImage(Id id, const QString& filename)
        : id(id), filename(filename), pixmap(QPixmap(filename))
{
}

InputImage::InputImage(const QString& str)
{
    if (!fromString(str)) {
        // TODO should throw an exception
    }
}

QString InputImage::toString() const
{
    QString str("%1,%2");

    return str.arg(QString::number(id), filename);
}

bool InputImage::fromString(const QString& str)
{
    QStringList list = str.split(",");

    if (list.size() == 2) {
        id       = list.at(0).toInt();
        filename = list.at(1);
        pixmap   = QPixmap(filename);
        return true;
    } else {
        qDebug() << "Warning InputImage::fromString(const QString&) invalid string format";
        return false;
    }
}
