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
    timer.defer(ClickTime, [] { digitalWrite(encoder_switch, HIGH); })
            .then([&] {
                _hasClicked = false;
                _clickToProcess = true;
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
        log_v("click -> %s", s ? "true" : "false");
        if (!s) { return; }
        auto steps = mnu_SdCard;
        while (steps-- > 1) {
            taskQueue.emplace(new MoveTask(1, nullptr));
        }
        taskQueue.emplace(new MoveTask(1, [this](bool s) {
            log_v("last move -> %s", s ? "true" : "false");
            if (!s) { return; }
            log_v("FOUND IT?");
            taskQueue.emplace(new ClickTask([this](bool s) {
                log_v("last click -> %s", s ? "true" : "false");
                if (!s) { return; }
                log_v("DONE");
            }));
        }));
    }));
}

void MenuManager::setTemp(string heater, uint8_t temp, function<void(bool)> cb) {
    // find correct heater
    // set
}

void MenuManager::attachUpdate() {
    screenBuffer.subUpdate([&](ScreenBuffer *, bool screenCleared) {
        auto buffer = screenBuffer.read();
        static uint8_t priorMenuLevel = 0;

        uint8_t offsets[] = {0, 20, 40, 60};
        if (buffer[0] == TEMP_SYMBOL) {
            menuLevel = 0;
            priorMenuLevel = 0;
            menuItem = "";
            clickGoesUp = false;
            _clickToProcess = false;
            return;
        }
        for (auto offset: offsets) {
//                Serial.printf("Offset: %u, Buffer: %2X\r\n", offset, buffer[offset]);
            if (buffer[offset] != ' ') {
                clickGoesUp = buffer[offset] == UP_ARROW;

                if (_clickToProcess) {
                    log_v("_clickToProcess == true");
                    _clickToProcess = false;
                    _hasClicked = true;
                    if (clickGoesUp) {
                        menuLevel--;
                    } else {
                        menuLevel++;
                    }
                }
                static uint8_t priorRow = 1;
                auto newRow = static_cast<uint8_t>(offset / 20 + 1);
                string newMenuItem = {buffer.begin() + offset + 1, buffer.begin() + offset + 20};

                // if we're at the bottom or top, we might be scrolling which is "moving"
                if (newRow == 4 || newRow == 1) {
                    // if we just moved down a level, menuItem will be empty
                    if (!menuItem.empty() && newMenuItem != menuItem) {
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
                        _hasMoved = true;
                    }
                } else { // normal moving (i.e. not scrolling)
                    moved = newRow - priorRow;
                    _hasMoved = true;
                }
                if (priorMenuLevel != menuLevel) {
                    log_v("menuLevel %u->%u", priorMenuLevel, menuLevel);
                    priorMenuLevel = menuLevel;
                    moved = 0;
                }

                log_v("moved: %i", moved);

                priorRow = newRow;
                menuItem = std::move(newMenuItem);
                break;
            }
        }
    });
}

int8_t MenuManager::hasMoved() {
    if (_hasMoved) {
        log_v("_hasMoved: %s", _hasMoved ? "true" : "false");
        _hasMoved = false;
        return moved;
    }

    return NO_MOVE;
}


bool MenuManager::hasClicked() {
    if (_hasMoved) {
        log_v("_hasMoved: %s, _hasClicked: %s", _hasMoved ? "true" : "false", _hasClicked ? "true" : "false");
        _hasMoved = false;
        if (_hasClicked) {
            _hasClicked = false;
            return true;
        }
    }

    return false;
}

void MenuManager::processQueue() {
    if (!taskQueue.empty()) {
        auto *task = taskQueue.front();
        taskQueue.pop();

        MenuTaskState state = MenuTaskState::Retry;
        do {
            if (state == MenuTaskState::Retry) { task->Run(*this); }
            delay(10);

            state = task->IsComplete(*this, millis());
        } while (state == MenuTaskState::Incomplete || state == MenuTaskState::Retry);

        task->executeCallback();
        log_v("Task %x complete.", task);
        delete task;
    }
}

MenuManager::MenuManager(ScreenBuffer &screenBuffer) noexcept
        : screenBuffer(screenBuffer), timer(TimerThing::Instance()) {
    pinMode(encoder_switch, OUTPUT);
    digitalWrite(encoder_switch, HIGH);
    pinMode(encoder_1, OUTPUT);
    digitalWrite(encoder_1, HIGH);
    pinMode(encoder_2, OUTPUT);
    digitalWrite(encoder_2, HIGH);
    attachUpdate();

    xTaskCreate([](void *o) {
        while (true) {
            static_cast<MenuManager *>(o)->processQueue();
            delay(100);
        }
    }, "mm_queue", 2048, this, 1, nullptr);
}
