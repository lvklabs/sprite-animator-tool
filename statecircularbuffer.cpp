#ifdef DEBUG_UNDO
#  include <QDebug>
#endif

#include "statecircularbuffer.h"


StateCircularBuffer::StateCircularBuffer()
    : _buf(0)
{
    _buf = new StateCircularBuffer::StateChange[BUFF_SIZE];

    clear();
}

StateCircularBuffer::~StateCircularBuffer()
{
    if (_buf) {
        delete[] _buf;
    }
}

void StateCircularBuffer::clear()
{
    _i = 0;
    _first = 0;
    _saved = 0;

    for (int i = 0; i < BUFF_SIZE; ++i) {
        _buf[i].type = st_null;
    }

    /* add a first null state to have an starting point */
    _buf[_i] = StateChange();

#ifdef DEBUG_UNDO
    qDebug() << " clear";
    qDebug() << toString();
#endif
}

void StateCircularBuffer::addState(const StateChange& st)
{
    /* do not add consecutive equal states */
    if (_i >= 0  && _buf[_i] == st) {
        return;
    }

    inc(_i);

    _buf[_i] = st;

    /* if the first state was overwritten */
    if (_i == _first) {
        inc(_first);
    }

    /* if the saved state was overwritten */
    if (_i == _saved) {
        _saved = -1;
    }

    /* overwrite all next states */
    if (hasNextState()) {
        int j = _i;
        while (inc(j) != _first && _buf[j].type != st_null) {
            _buf[j].type = st_null;
            if (j == _saved) {
                _saved = -1;
            }
        }
    }

#ifdef DEBUG_UNDO
        qDebug() << " addState type" << st.type;
        qDebug() << toString();
#endif
}

StateCircularBuffer::StateChange StateCircularBuffer::currentState() const
{
    StateCircularBuffer::StateChange st;

    if (_i != -1) {
        st = _buf[_i];
    }
    return st;
}

bool StateCircularBuffer::hasNextState() const
{
    int next = _i;
    inc(next);

    if (next == _first) {
        return false;
    } else {
        return _buf[next].type != st_null;
    }
}

bool StateCircularBuffer::hasPrevState() const
{
    return _i != _first;
}

void StateCircularBuffer::nextState()
{
    if (hasNextState()) {
        inc(_i);
    }

#ifdef DEBUG_UNDO
        qDebug() << " nextState";
        qDebug() << toString();
#endif
}

void StateCircularBuffer::prevState()
{
    if (hasPrevState()) {
        dec(_i);
    }

#ifdef DEBUG_UNDO
        qDebug() << " prevState";
        qDebug() << toString();
#endif
}

void StateCircularBuffer::setSavedFlag()
{
    _saved = _i;

#ifdef DEBUG_UNDO
        qDebug() << " prevState";
        qDebug() << toString();
#endif
}

bool StateCircularBuffer::hasSavedFlag() const
{
    return _saved == _i;
}

QString StateCircularBuffer::toString() const
{
    QString str;

//    str.append("\n  ");
    str.append(" ");
    for (int i = 0; i < BUFF_SIZE; ++i) {
        if (_buf[i].type == st_transactionStart) {
            str.append("(");
        } else {
            str.append(" ");
        }
        str.append(QString::number(i, 16));
        if (_buf[i].type == st_transactionEnd) {
            str.append(") ");
        } else {
            str.append("  ");
        }
    }

    str.append("\n | ");
    for (int i = 0; i < BUFF_SIZE; ++i) {
        if (_buf[i].type != st_null) {
            str.append(QString::number(_buf[i].type, 16));
        } else {
            str.append(" ");
        }
        str.append(" | ");
    }

    str.append("\n  ");
    for (int i = 0; i < BUFF_SIZE; ++i) {
        if (i == _saved) {
            str.append("s");
        } else {
            str.append(" ");
        }
        if (i == _i) {
            str.append("i");
        } else {
            str.append(" ");
        }
        if (i == _first) {
            str.append("f ");
        } else {
            str.append("  ");
        }
    }
    return str;
}
