#include "Thing.h"
#include "TimerThing.h"

Thing Thing::then(void(*lambda)()) {
    return timerThing->defer_abs(when + 1, lambda);
}

Thing Thing::then(const unsigned long _millis, void(*lambda)()) {
    return timerThing->defer_abs(when + _millis, lambda);
}

Thing::Thing(TimerThing *timerThing, const unsigned long when, void(*lambda)())
        : when(when), lambda(lambda), timerThing(timerThing) {}
