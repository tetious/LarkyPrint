#pragma once

#include "Arduino.h"
#include "Thing.h"
#include "TimerThing.h"
#include "pins.h"
#include "bits/stdc++.h"
#include <esp32-hal-log.h>

const uint8_t TEMP_SYMBOL = 0x02, UP_ARROW = 0x03;

class MenuManager;

struct Mover {
    int8_t count;
    int8_t retries = 0;
    function<void(bool)> cb;

    Mover(int8_t count, function<void(bool)> cb) : count(count), cb(std::move(cb)) {}
};


class MenuManager {
    const uint8_t EncoderTickLength = 20;
    const uint8_t EncoderMoveTime = EncoderTickLength / 2 * 3;
    const int8_t mnu_SdCard = 3;
    const int8_t mnu_Control = 2;
    const std::list<int8_t> mnu_Temp = {mnu_Control, 1};

    void _up(const function<void()> &done = nullptr) {
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

    void _down(const function<void()> &done = nullptr) {
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

public:
    explicit MenuManager(ScreenBuffer &screenBuffer) : screenBuffer(screenBuffer) {
        pinMode(encoder_switch, OUTPUT);
        digitalWrite(encoder_switch, HIGH);
        pinMode(encoder_1, OUTPUT);
        digitalWrite(encoder_1, HIGH);
        pinMode(encoder_2, OUTPUT);
        digitalWrite(encoder_2, HIGH);
        attachUpdate();
    }

    // todo: rework so up and down are reliable even with buggy encoder config

    void learn() {
        // TODO: find Print to SD Card
    }

    void click() {
        menuItem = "";

        digitalWrite(encoder_switch, LOW);
        timer.defer(1, [] { digitalWrite(encoder_switch, HIGH); })
                .then([&] {
                    clickGoesUp ? menuLevel-- : menuLevel++;
                });
    }

    void up() {
        if (menuEncoderReversed) {
            _down();
        } else {
            _up();
        }
    }

    void down() {
        if (menuEncoderReversed) {
            _up();
        } else {
            _down();
        }
    }

    void move(int8_t count, const function<void(bool)> &cb) {

    }

    void moveTo(const list <int8_t> path, const function<void(bool)> &cb) {
        for (auto i: path) {
            click();
            //if (!move(i)) { cb(false); }
        }
        if (cb) { cb(true); }
    }

    void find(const string &item, const function<void(bool)> &cb) {
        log_v("Looking for item: %s.", item.c_str());
        if (cb == nullptr) { log_e("find cb is null"); }

        if (activeMover != nullptr) {
            log_e("activeMover is not null!");
            return;
        }

        activeMover = new Mover{1, [&](bool s) {
            if (item.substr(0, menuItem.size()) == menuItem) {
                log_v("Found item: %s!", item.c_str());
                if (cb) { cb(true); }
            } else {
                if (s) {
                    log_v("Still looking for item: %s...", item.c_str());
                    find(item, cb);
                } else {
                    if (cb) { cb(false); }
                }
            }
        }};
        startMove();
    }

    void printFile(string filename, const function<void(bool)> &cb) {
        if (activeMover != nullptr) {
            log_e("activeMover is not null!");
            return;
        }

        click();
        delay(1000);

        activeMover = new Mover{mnu_SdCard, [=](bool s) {
            click();
            //find(filename, cb);
        }};
        startMove();
    }

    void setTemp(string heater, uint8_t temp, function<void(bool)> cb) {
        // find correct heater
        // set
    }

    int8_t moved = 0;
    function<void()> onMove = nullptr;

private:
    ScreenBuffer &screenBuffer;
    TimerThing &timer = TimerThing::Instance();
    Mover *activeMover = nullptr;
    bool menuEncoderReversed = true;

    uint8_t menuLevel = 0;
    bool clickGoesUp = false;
    string menuItem = "";

    void startMove() {
        onMove = [&] { this->_move_moved(); };
        _moveOnce();
    }

    void _moveOnce() {
        log_v("count: %i", activeMover->count);
        if (activeMover->count > 0) {
            log_v("moving down");
            down();
        } else {
            log_v("moving up");
            up();
        }
    }

    void _move_moved() {
        activeMover->count -= moved;
        activeMover->retries += moved == 0 ? 1 : 0;
        log_v("count: %i, retries: %i", activeMover->count, activeMover->retries);
        if (activeMover->count != 0 && activeMover->retries < 3) {
            _moveOnce();
            return;
        }

        onMove = nullptr;
        activeMover->cb(activeMover->count == 0);
        delete activeMover;
        activeMover = nullptr;
    }


    void attachUpdate() {
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
                                if (onMove) { onMove(); }
                            } else {
                                moved = -1;
                                if (onMove) { onMove(); }
                            }
                        } else {
                            moved = 0;
                            if (onMove) { onMove(); }
                        }
                    } else { // normal moving (i.e. not scrolling)
                        moved = newRow - priorRow;
                        if (onMove) { onMove(); }
                    }
                    priorRow = newRow;
                    menuItem = std::move(newMenuItem);
                    break;
                }
            }
        });
    }
};
