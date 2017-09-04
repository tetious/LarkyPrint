#pragma once

#include <functional>

using namespace std;

class TimerThing;

class Thing {
public:
    const unsigned long when;

    Thing(TimerThing *timerThing, unsigned long when, function<void()> lambda);

    function<void()> lambda;

    Thing then(function<void()> lambda);

    Thing then(unsigned long _millis, function<void()> lambda);

private:
    TimerThing *timerThing;
};