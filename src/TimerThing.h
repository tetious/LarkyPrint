#pragma once
#include <vector>
#include <functional>

class Thing;

class TimerThing {
    std::vector<Thing> things;

public:
    Thing defer(unsigned long _millis, void(*lambda)());

    Thing defer_abs(unsigned long _millis, void(*lambda)());

    void loop(unsigned long _millis);
};