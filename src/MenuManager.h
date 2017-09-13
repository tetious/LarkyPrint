#pragma once

#include "Arduino.h"
#include "Thing.h"
#include "TimerThing.h"
#include "pins.h"

const uint8_t TEMP_SYMBOL = 0x02, UP_ARROW = 0x03;

class MenuManager {
    const unsigned long EncoderTickLength = 30;

    void _up() {
        digitalWrite(encoder_1, LOW);
        timer.defer(EncoderTickLength / 2, [] {
            digitalWrite(encoder_2, LOW);
        }).then(EncoderTickLength / 2, [] {
            digitalWrite(encoder_1, HIGH);
        }).then(EncoderTickLength / 2, [] {
            digitalWrite(encoder_2, HIGH);
        });
    }

    void _down() {
        digitalWrite(encoder_2, LOW);
        timer.defer(EncoderTickLength / 2, [] {
            digitalWrite(encoder_1, LOW);
        }).then(EncoderTickLength / 2, [] {
            digitalWrite(encoder_2, HIGH);
        }).then(EncoderTickLength / 2, [] {
            digitalWrite(encoder_1, HIGH);
        });
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
        digitalWrite(encoder_switch, LOW);

        auto thing = timer.defer(100, [] { digitalWrite(encoder_switch, HIGH); })
                .then(100, [&] {
                    clickGoesUp ? menuLevel-- : menuLevel++;
                });
    }

    void up() {
        if(menuEncoderReversed) {
            _down();
        } else {
            _up();
        }
    }

    void down() {
        if(menuEncoderReversed) {
            _up();
        } else {
            _down();
        }
    }

private:
    ScreenBuffer &screenBuffer;
    TimerThing &timer = TimerThing::Instance();

    bool menuEncoderReversed = true;

    uint8_t menuLevel = 0;
    bool clickGoesUp = false;
    uint8_t menuRow = 0;

    void attachUpdate() {
        screenBuffer.subUpdate([&](ScreenBuffer *) {
            auto buffer = screenBuffer.read();
            uint8_t offsets[] = {0, 20, 40, 60};
            if (buffer[0] == TEMP_SYMBOL) {
                menuLevel = 0;
                return;
            }
            for (auto offset: offsets) {
//                Serial.printf("Offset: %u, Buffer: %2X\r\n", offset, buffer[offset]);
                if (buffer[offset] == UP_ARROW) {
                    clickGoesUp = true;
                    menuRow = 1;
                    break;
                } else if (buffer[offset] == '>') {
                    clickGoesUp = false;
                    menuRow = static_cast<uint8_t>(offset / 20 + 1);
                    break;
                }
            }

            Serial.printf("Menu row: %u", menuRow);
        });
    }
};