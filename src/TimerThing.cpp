#include <algorithm>
#include <functional>
#include "Thing.h"
#include "TimerThing.h"
#include "Arduino.h"

using namespace std;

unsigned int TimerThing::setTimeout(const unsigned long _millis, function<void()> lambda) {
    timeouts.push_back({_millis, move(lambda)});
    return timeouts.size();
}

void TimerThing::clearTimeout(unsigned int id) {
    timeouts.erase(timeouts.begin() + id);
}


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

    for (auto timeout: timeouts) {
        if (_millis % timeout.timeout == 0) { timeout.lambda(); }
    }
}

TimerThing &TimerThing::Instance() {
    static TimerThing instance;
    return instance;
}

TimerThing::TimerThing() {
    xTaskCreate([](void *o) {
        TickType_t lastWakeTime;
        const auto freq = 1 / portTICK_PERIOD_MS;
        while (true) {
            lastWakeTime = xTaskGetTickCount();
            static_cast<TimerThing *>(o)->loop(millis());
            vTaskDelayUntil(&lastWakeTime, freq);
        }
    }, "tt_loop", 2048, this, 1, nullptr);
}
