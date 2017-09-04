#include "Thing.h"

#include "TimerThing.h"

using namespace std;

Thing Thing::then(function<void()> lambda) {
    return timerThing->defer_abs(when + 1, move(lambda));
}

Thing Thing::then(const unsigned long _millis, function<void()> lambda) {
    return timerThing->defer_abs(when + _millis, move(lambda));
}

Thing::Thing(TimerThing *timerThing, const unsigned long when, function<void()> lambda)
        : when(when), lambda(move(lambda)), timerThing(timerThing) {}
