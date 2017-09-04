#pragma once

#include <vector>
#include <functional>

using namespace std;

class Thing;

class TimerThing {
    vector<Thing> things{};

public:
    Thing defer(unsigned long _millis, function<void()> lambda);

    Thing defer_abs(unsigned long _millis, function<void()> lambda);

    void loop(unsigned long _millis);
};