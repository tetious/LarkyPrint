#pragma once

#include <vector>
#include <functional>

using namespace std;

class Thing;



class TimerThing {
    struct Timeout {
        unsigned long timeout;
        function<void()> lambda;
    };

    vector<Thing> things;
    vector<Timeout> timeouts;

public:
    Thing defer(unsigned long _millis, function<void()> lambda);

    Thing defer_abs(unsigned long _millis, function<void()> lambda);

    void loop(unsigned long _millis);

    static TimerThing &Instance();

    TimerThing(TimerThing const &) = delete;

    TimerThing &operator=(TimerThing const &) = delete;

protected:
    TimerThing();

    unsigned int setTimeout(unsigned long _millis, function<void()> lambda);

    void clearTimeout(unsigned int id);
};