#pragma once

#include "Arduino.h"
#include "Thing.h"
#include "TimerThing.h"

const uint8_t EN_SW = 21, EN_1 = 5, EN_2 = 18;

const uint8_t TEMP_SYMBOL = 0x02, UP_ARROW = 0x03;

class MenuManager {
public:
    explicit MenuManager(ScreenBuffer &screenBuffer) : screenBuffer(screenBuffer) {
        pinMode(EN_SW, OUTPUT);
        digitalWrite(EN_SW, HIGH);
        pinMode(EN_1, OUTPUT);
        pinMode(EN_2, OUTPUT);
    }

    void learn() {

    }

    void click() {
        digitalWrite(EN_SW, LOW);

        auto thing = timer.defer(100, [] { digitalWrite(EN_SW, HIGH); })
                .then(100, [&] {
                    update();
                    clickGoesUp ? menuLevel-- : menuLevel++;
                });
    }

    void up() {
        digitalWrite(EN_1, HIGH);
        timer.defer(EncoderTickLength, [] {
            digitalWrite(EN_1, LOW);
            digitalWrite(EN_2, HIGH);
        }).then(EncoderTickLength, [] {
            digitalWrite(EN_2, LOW);
        }).then(100, [&] { update(); });
    }

    void down() {
        digitalWrite(EN_2, HIGH);
        timer.defer(EncoderTickLength, [] {
            digitalWrite(EN_2, LOW);
            digitalWrite(EN_1, HIGH);
        }).then(EncoderTickLength, [] {
            digitalWrite(EN_1, LOW);
        }).then(100, [&] { update(); });
    }

    void loop(const unsigned long _millis) {
        timer.loop(_millis);
    }

private:
    ScreenBuffer &screenBuffer;
    TimerThing timer{};
    const unsigned long EncoderTickLength = 10;

    uint8_t menuLevel = 0;
    bool clickGoesUp = false;
    uint8_t menuRow = 0;

    void update() {
        auto buffer = screenBuffer.read();
        for (auto chr : buffer) {
            if (chr > 31) Serial.write(chr);
        }
        Serial.println();
        uint8_t offsets[] = {0, 20, 40, 60};
        if (buffer[0] == TEMP_SYMBOL) {
            Serial.println("MenuManager::update -> On info screen.");
            menuLevel = 0;
            return;
        }
        for (auto offset: offsets) {
            Serial.printf("Offset: %u, Buffer: %2X\r\n", offset, buffer[offset]);
            if (buffer[offset] == UP_ARROW) {
                clickGoesUp = true;
                Serial.println("UP ARROW");
                menuRow = 1;
                break;
            } else if (buffer[offset] == '>') {
                clickGoesUp = false;
                menuRow = static_cast<uint8_t>(offset / 20 + 1);
                break;
            }
        }
        Serial.printf("Menu row: %u", menuRow);
    }
};