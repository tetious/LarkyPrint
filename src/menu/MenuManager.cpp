#include "Thing.h"
#include "TimerThing.h"
#include "MenuManager.h"
#include "ScreenBuffer.h"
#include "MenuTask.h"

using namespace std;

void MenuManager::_up(const function<void()> &done) {
    digitalWrite(encoder_1, LOW);
    auto chain = timer.defer(EncoderTickLength / 2, [] {
        digitalWrite(encoder_2, LOW);
    }).then(EncoderTickLength / 2, [] {
        digitalWrite(encoder_1, HIGH);
    }).then(EncoderTickLength / 2, [] {
        digitalWrite(encoder_2, HIGH);
    });
    if (done != nullptr) {
        chain.then(done);
    }
}

void MenuManager::_down(const function<void()> &done) {
    digitalWrite(encoder_2, LOW);
    auto chain = timer.defer(EncoderTickLength / 2, [] {
        digitalWrite(encoder_1, LOW);
    }).then(EncoderTickLength / 2, [] {
        digitalWrite(encoder_2, HIGH);
    }).then(EncoderTickLength / 2, [] {
        digitalWrite(encoder_1, HIGH);
    });
    if (done != nullptr) {
        chain.then(done);
    }
}

void MenuManager::click() {
    menuItem = "";

    digitalWrite(encoder_switch, LOW);
    timer.defer(5, [] { digitalWrite(encoder_switch, HIGH); })
            .then([&] {
                clickGoesUp ? menuLevel-- : menuLevel++;
                _hasClicked = true;
            });
}

void MenuManager::up() {
    if (menuEncoderReversed) {
        _down();
    } else {
        _up();
    }
}

void MenuManager::down() {
    if (menuEncoderReversed) {
        _up();
    } else {
        _down();
    }
}

void MenuManager::moveTo(const list<int8_t> path, const function<void(bool)> &cb) {
    for (auto i: path) {
        click();
        //if (!move(i)) { cb(false); }
    }
    if (cb) { cb(true); }
}

void MenuManager::find(const string &item, function<void(bool)> cb, bool recursing = false) {
    /*log_v("Looking for item: %s.", item.c_str());
    if (cb == nullptr) { log_e("find cb is null"); }
    activeFinder = std::move(cb);
    if (!recursing && activeMover != nullptr) {
        log_e("activeMover is not null!");
        cleanupMover();
    }

    activeMover = new Mover{1, [=](bool s) {
        if (item.substr(0, menuItem.size()) == menuItem) {
            log_v("Found item: %s!", item.c_str());
            if (activeFinder) { activeFinder(true); }
            activeFinder = nullptr;
        } else {
            if (s) {
                log_v("Still looking for item: %s...", item.c_str());
                find(item, activeFinder, true);
            } else {
                if (activeFinder) { activeFinder(false); }
                activeFinder = nullptr;
            }
        }
    }};
    startMove();*/
}

void MenuManager::printFile(string filename, const function<void(bool)> &cb) {
    log_v("printing filename: %s.", filename.c_str());

    taskQueue.emplace(new ClickTask([this](bool s) {
        if (!s) { return; }
        auto steps = mnu_SdCard;
        while (steps-- > 1) {
            taskQueue.emplace(new MoveTask(nullptr));
        }
        taskQueue.emplace(new MoveTask([this](bool s) {
            if (!s) { return; }
            log_e("FOUND IT?");
            taskQueue.emplace(new ClickTask([this](bool s) {
                if (!s) { return; }
                log_e("DONE");
            }));
        }));
    }));
}

void MenuManager::setTemp(string heater, uint8_t temp, function<void(bool)> cb) {
    // find correct heater
    // set
}

void MenuManager::attachUpdate() {
    screenBuffer.subUpdate([&](ScreenBuffer *) {
        auto buffer = screenBuffer.read();
        uint8_t offsets[] = {0, 20, 40, 60};
        if (buffer[0] == TEMP_SYMBOL) {
            menuLevel = 0;
            menuItem = "";
            return;
        }
        for (auto offset: offsets) {
//                Serial.printf("Offset: %u, Buffer: %2X\r\n", offset, buffer[offset]);
            if (buffer[offset] != ' ') {
                clickGoesUp = buffer[offset] == UP_ARROW;
                static uint8_t priorRow{};
                uint8_t newRow = static_cast<uint8_t>(offset / 20 + 1);
                string newMenuItem = {buffer.begin() + offset + 1, buffer.begin() + offset + 20};

                // if we're at the bottom or top, we might be scrolling which is "moving"
                if (newRow == 4 || newRow == 1) {
                    // if we just moved down a level, menuItem will be empty
                    if (menuItem != "" && newMenuItem != menuItem) {
                        // we scrolled, so that's moving
                        if (newRow == 4) {
                            moved = 1;
                            _hasMoved = true;
                        } else {
                            moved = -1;
                            _hasMoved = true;
                        }
                    } else {
                        moved = 0;
                    }
                } else { // normal moving (i.e. not scrolling)
                    moved = newRow - priorRow;
                    _hasMoved = true;
                }
                priorRow = newRow;
                menuItem = std::move(newMenuItem);
                break;
            }
        }
    });
}

int8_t MenuManager::hasMoved() {
    log_v("_hasMoved: %s", _hasMoved ? "true" : "false");
    if (_hasMoved) {
        _hasMoved = false;
        return moved;
    }

    return NO_MOVE;
}


bool MenuManager::hasClicked() {
    log_v("_hasMoved: %s, _hasClicked: %s", _hasMoved ? "true" : "false", _hasClicked ? "true" : "false");
    if (_hasMoved) {
        _hasMoved = false;
        _hasClicked = false;
        return _hasClicked;
    }

    return false;
}

void MenuManager::processQueue(const unsigned long millis) {
    if (!taskQueue.empty()) {
        auto *task = taskQueue.front();
        taskQueue.pop();

        MenuTaskState state = MenuTaskState::Retry;
        do {
            if (state == MenuTaskState::Retry) { task->Run(*this); }
            state = task->IsComplete(*this, millis);
        } while (state == MenuTaskState::Incomplete || state == MenuTaskState::Retry);

        task->executeCallback();
        delete task;
    }
}

MenuManager::MenuManager(ScreenBuffer &screenBuffer)
        : screenBuffer(screenBuffer), timer(TimerThing::Instance()) {
    pinMode(encoder_switch, OUTPUT);
    digitalWrite(encoder_switch, HIGH);
    pinMode(encoder_1, OUTPUT);
    digitalWrite(encoder_1, HIGH);
    pinMode(encoder_2, OUTPUT);
    digitalWrite(encoder_2, HIGH);
    attachUpdate();

    xTaskCreate([](void *o) {
        TickType_t lastWakeTime;
        const auto freq = 1 / portTICK_PERIOD_MS;
        while (true) {
            lastWakeTime = xTaskGetTickCount();
            static_cast<MenuManager *>(o)->processQueue(millis());
            vTaskDelayUntil(&lastWakeTime, freq);
        }
    }, "mm_queue", 2048, this, 1, nullptr);
}
