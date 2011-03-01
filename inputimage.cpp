#include <QStringList>
#include <QDebug>

#include "inputimage.h"

InputImage::InputImage(Id id, const QString& filename, double scale)
        : id(id), filename(filename), pixmap(QPixmap(filename)), _scale(scale)
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
    QString str("%1,%2,%3");

    return str.arg(QString::number(id), filename, QString::number(_scale));
}

bool InputImage::fromString(const QString& str)
{
    QStringList list = str.split(",");

    if (list.size() >= 2 && list.size() <= 3) {
        id       = list.at(0).toInt();
        filename = list.at(1);
        _scale   = (list.size() >= 3) ? list.at(2).toDouble() : 1.0;

        if (_scale == 1.0) {
            pixmap = QPixmap(filename);
        } else {
            scale(_scale);
        }

        if (pixmap.isNull()) {
            qDebug() << "Warning InputImage::fromString(const QString&) null pixmap created from file"
                     << filename;
        }
        return true;
    } else {
        qDebug() << "Warning InputImage::fromString(const QString&) invalid string format";
        return false;
    }
}

void InputImage::scale(double scale)
{
    _scale = scale;

    QPixmap origPixmap(filename);
    if (!origPixmap.isNull()) {
        int w = origPixmap.width()*_scale;
        int h = origPixmap.height()*_scale;
        pixmap = origPixmap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        pixmap = QPixmap();
    }
}

double InputImage::scale() const
{
    return _scale;
}

void InputImage::reloadImage()
{
    scale(_scale);
}

void InputImage::freeImageData()
{
    // TODO check if this actually does something
    pixmap = QPixmap();
}

