#pragma once

#include <functional>

class TimerThing;

class Thing {
public:
    unsigned long when;
    std::function<void()> lambda;

    Thing(TimerThing *timerThing, unsigned long when, std::function<void()> lambda);

    Thing then(std::function<void()> lambda);

    Thing then(unsigned long _millis, std::function<void()> lambda);

private:
    TimerThing *timerThing;
};