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

LvkAframe& LvkAnimation::aframe(Id aframeId)
{
    static LvkAframe nullAframe;

    for (int i = 0; i < _aframes.size(); ++i) {
        if (_aframes[i].id == aframeId) {
            return _aframes[i];
        }
    }
    return nullAframe;
}

void LvkAnimation::addAframe(const LvkAframe &aframe)
{
    _aframes.push_back(aframe);
}

void LvkAnimation::removeAframe(Id aframeId)
{
    for (int i = 0; i < _aframes.size(); ++i) {
        if (_aframes[i].id == aframeId) {
            _aframes.removeAt(i);
        }
    }
}

void LvkAnimation::swapAframes(Id aframeId1, Id aframeId2)
{
    for (int k = 0, i = -1, j = -1; k < _aframes.size(); ++k) {
        if (_aframes[k].id == aframeId1) {
            i = k;
        } else if (_aframes[k].id == aframeId2) {
            j = k;
        }
        if (i != -1 && j != -1) {
            _aframes.swap(i, j);
            break;
        }
    }
}
