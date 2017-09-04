#include <algorithm>
#include <functional>
#include "Thing.h"
#include "TimerThing.h"
#include "Arduino.h"

using namespace std;

Thing TimerThing::defer(const unsigned long _millis, function<void()> lambda) {
    return defer_abs(_millis + millis(), move(lambda));
}

Thing TimerThing::defer_abs(const unsigned long _millis, function<void()> lambda) {
    auto thing = Thing{this, _millis, move(lambda)};
    things.push_back(thing);
    return thing;
}

void TimerThing::loop(const unsigned long _millis) {
    vector<Thing> triggeredThings;
    vector<Thing> new_first;

    partition_copy(things.begin(),
                   things.end(),
                   back_inserter(triggeredThings),
                   back_inserter(new_first),
                   [&](Thing it) { return it.when <= _millis; });
    things = move(new_first);

    for (auto thing : triggeredThings) {
        if (thing.lambda == nullptr) {
            Serial.println("NULL! :(");
        } else {
            thing.lambda();
        }
    }
}
