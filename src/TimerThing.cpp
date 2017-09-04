#include <algorithm>
#include <functional>
#include <utility>
#include "Thing.h"
#include "TimerThing.h"
#include "Arduino.h"

Thing TimerThing::defer(const unsigned long _millis, void(*lambda)()) {
    return defer_abs(_millis + millis(), lambda);
}

Thing TimerThing::defer_abs(const unsigned long _millis, void(*lambda)()) {
    auto thing = Thing{this, _millis, lambda};
    things.push_back(thing);
    return thing;
}

void TimerThing::loop(const unsigned long _millis) {
    std::vector <Thing> triggeredThings;
    std::vector<Thing> new_first;
    std::partition_copy(std::make_move_iterator(things.begin()),
                        std::make_move_iterator(things.end()),
                        std::back_inserter(triggeredThings),
                        std::back_inserter(new_first),
                        [&](Thing it) { return it.when <= _millis; });
    things = std::move(new_first);

    for (auto thing : triggeredThings) {
        Serial.println("Triggered!");
        thing.lambda();
    }
}
