#include <algorithm>
#include <functional>
#include <utility>
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
    static auto inLoop = false;
    if (!inLoop) {
        inLoop = true;

        auto it = things.begin();
        while (it != things.end()) {
            if(it->when <= _millis) {
                it->lambda();
                it = things.erase(it);
            } else {
                it++;
            }
        }

        for (auto timeout: timeouts) {
            if (_millis % timeout.timeout == 0) { timeout.lambda(); }
        }

        inLoop = false;
    } else {
        Serial.printf("TimerThing::loop while already looping [%u]!\r\n", _millis);
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
    }, "tt_loop", 2048, this, 2, nullptr);
}
