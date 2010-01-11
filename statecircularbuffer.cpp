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

void StateCircularBuffer::addState(StateChange st)
{
    inc(_i);

    _buf[_i] = st;

    if (_first == -1 || _i == _first) {
        inc(_first);
    }

    if (hasNextState()) {
        int tmp = _i;
        //while (inc(tmp) != _first && _buf[tmp].type != st_null) {
            _buf[tmp].type = st_null;
        //}
    }
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
}

void StateCircularBuffer::prevState()
{
    if (hasPrevState()) {
        dec(_i);
    }
}