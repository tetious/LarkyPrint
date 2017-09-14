#include "Thing.h"

#include <utility>

#include "TimerThing.h"

using namespace std;

Thing Thing::then(function<void()> lambda) {
    return timerThing->defer_abs(when + 1, std::move(lambda));
}

Thing Thing::then(const unsigned long _millis, function<void()> lambda) {
    return timerThing->defer_abs(when + _millis, std::move(lambda));
}

Thing::Thing(TimerThing *timerThing, const unsigned long when, function<void()> lambda)
        : when(when), lambda(std::move(lambda)), timerThing(timerThing) {}
