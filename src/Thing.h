#pragma once
#include <functional>

class TimerThing;

class Thing {
public:
    const unsigned long when;

    Thing(TimerThing *timerThing, unsigned long when, void(*lambda)());

    void(*lambda)();

    Thing then(void(*lambda)());

    Thing then(unsigned long _millis, void(*lambda)());

private:
    TimerThing *timerThing;
};