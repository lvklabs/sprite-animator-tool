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
    _i = -1;
    _first = -1;

    for (int i = 0; i < BUFF_SIZE; ++i) {
        _buf[i].type = st_null;
    }

    /* cheat: add a first null state to have an starting point */
    addState(StateChange());
}

void StateCircularBuffer::addState(const StateChange& st)
{
    if (_i >= 0  && _buf[_i] == st) {
        return;
    }

    inc(_i);

    _buf[_i] = st;

    if (_first == -1 || _i == _first) {
        inc(_first);
    }

    if (hasNextState()) {
        int tmp = _i;
        while (inc(tmp) != _first && _buf[tmp].type != st_null) {
            _buf[tmp].type = st_null;
        }
    }

#ifdef DEBUG_UNDO
        qDebug() << " addState type" << st.type;
        qDebug() << toString();
#endif
}

StateCircularBuffer::StateChange StateCircularBuffer::currentState()
{
    StateCircularBuffer::StateChange st;

    if (_i != -1) {
        st = _buf[_i];
    }
    return st;
}

bool StateCircularBuffer::hasNextState()
{
    int next = _i;
    inc(next);

    if (next == _first) {
        return false;
    } else {
        return _buf[next].type != st_null;
    }
}

bool StateCircularBuffer::hasPrevState()
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

QString StateCircularBuffer::toString()
{
    QString str;

//    str.append("\n  ");
    str.append(" ");
    for (int i = 0; i < BUFF_SIZE; ++i) {
        str.append(" ");
        str.append(QString::number(i, 16));
        str.append("  ");
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
        str.append(" ");
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
