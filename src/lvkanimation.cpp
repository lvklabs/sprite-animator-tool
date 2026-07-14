#include <QDebug>
#include <QStringList>

#include <utility>

#include "lvkanimation.h"
#include "spritestate.h"

LvkAnimation::LvkAnimation(Id id, const QString &name, unsigned flags)
    : id(id), name(name), flags(flags) {}

LvkAnimation::LvkAnimation(const QString &str) {
    if (!fromString(str)) {
        // TODO should throw an exception
    }
}

QString LvkAnimation::toString() const {
    return toString(LvkVersion::V_04);
}

QString LvkAnimation::toString(LvkVersion v) const {
    // Phase 6b (Item 26): CSV format uses ',' as the field separator and has
    // no escape mechanism. A name containing a literal ',' (or NUL) would
    // corrupt the on-disk record and re-load as garbage. Refuse to serialize
    // rather than silently truncate or escape -- the simpler invariant lets
    // the round-trip stay byte-identical for valid inputs.
    if (name.contains(QLatin1Char(',')) || name.contains(QChar('\0'))) {
        qWarning() << "LvkAnimation::toString refusing to serialize name containing comma or NUL:"
                   << name;
        return QString();
    }

    // flags column was added in v0.3.
    if (v < LvkVersion::V_03) {
        return QStringLiteral("%1,%2").arg(QString::number(id), name);
    }
    return QStringLiteral("%1,%2,%3").arg(QString::number(id), name, QString::number(flags));
}

bool LvkAnimation::fromString(const QString &str) {
    QStringList list = str.split(",");

    if (list.size() == 2) {
        id = list.at(0).toInt();
        name = list.at(1);
        flags = 0;
        return true;
    } else if (list.size() == 3) {
        id = list.at(0).toInt();
        name = list.at(1);
        // flags is unsigned and serialized with QString::number(unsigned):
        // toInt() would overflow (and return 0) for values > INT_MAX,
        // silently zeroing high-bit flags on a plain load/save round-trip.
        flags = list.at(2).toUInt();
        return true;
    } else {
        qDebug() << "Warning LvkAnimation::fromString(const QString&) invalid string format";
        return false;
    }
}

bool LvkAnimation::hasAframe(Id aframeId) const {
    for (int i = 0; i < _aframes.size(); ++i) {
        if (_aframes.at(i).id == aframeId) {
            return true;
        }
    }
    return false;
}

LvkAframe *LvkAnimation::findAframe(Id aframeId) {
    for (int i = 0; i < _aframes.size(); ++i) {
        if (_aframes[i].id == aframeId) {
            return &_aframes[i];
        }
    }
    return nullptr;
}

const LvkAframe &LvkAnimation::aframe(Id aframeId) const {
    // Agent 7: const variant. Use a static empty-aframe sentinel for the
    // not-found case; the returned reference remains valid for the
    // lifetime of the program.
    static const LvkAframe nullAframe;

    for (int i = 0; i < _aframes.size(); ++i) {
        if (_aframes.at(i).id == aframeId) {
            return _aframes.at(i);
        }
    }
    return nullAframe;
}

void LvkAnimation::addAframe(const LvkAframe &aframe) {
    _aframes.push_back(aframe);
}

void LvkAnimation::removeAframe(Id aframeId) {
    // Single-removal semantics ("the" aframe with this id). Without the
    // break, after removeAt(i) shifts the tail down, the loop's ++i skips
    // what is now element i (formerly i+1). For uniquely-id'd aframes this
    // is unobservable; if duplicates ever existed (test fixtures, bad
    // input) the second match would be silently skipped. Break after the
    // first match so the behaviour matches the singular API name.
    for (int i = 0; i < _aframes.size(); ++i) {
        if (_aframes[i].id == aframeId) {
            _aframes.removeAt(i);
            break;
        }
    }
}

void LvkAnimation::swapAframes(Id aframeId1, Id aframeId2) {
    // Swap the CONTENT of the two aframes but keep the ids position-stable
    // (i.e. re-swap the id fields after swapping the elements). Playback
    // and JSON export follow in-memory list order, but save() serializes
    // aframes sorted by id, so an id must always denote a list position:
    // if the ids rode along with the swapped elements, the reorder would
    // be silently undone by the next save/load round-trip.
    for (int k = 0, i = -1, j = -1; k < _aframes.size(); ++k) {
        if (_aframes[k].id == aframeId1) {
            i = k;
        } else if (_aframes[k].id == aframeId2) {
            j = k;
        }
        if (i != -1 && j != -1) {
            _aframes.swapItemsAt(i, j);
            std::swap(_aframes[i].id, _aframes[j].id);
            break;
        }
    }
}
