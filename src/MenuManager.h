#pragma once

#include "Arduino.h"
#include "Thing.h"
#include "TimerThing.h"
#include "pins.h"

const uint8_t TEMP_SYMBOL = 0x02, UP_ARROW = 0x03;

class MenuManager {
    const unsigned long EncoderTickLength = 30;

public:
    explicit MenuManager(ScreenBuffer &screenBuffer) : screenBuffer(screenBuffer) {
        pinMode(encoder_switch, OUTPUT);
        digitalWrite(encoder_switch, HIGH);
        pinMode(encoder_1, OUTPUT);
        digitalWrite(encoder_1, HIGH);
        pinMode(encoder_2, OUTPUT);
        digitalWrite(encoder_2, HIGH);
    }

    void learn() {

    }

    void click() {
        digitalWrite(encoder_switch, LOW);

        auto thing = timer.defer(100, [] { digitalWrite(encoder_switch, HIGH); })
                .then(100, [&] {
                    update();
                    clickGoesUp ? menuLevel-- : menuLevel++;
                });
    }

    void up() {
        digitalWrite(encoder_1, LOW);
        timer.defer(EncoderTickLength / 2, [] {
            digitalWrite(encoder_2, LOW);
        }).then(EncoderTickLength / 2, [] {
            digitalWrite(encoder_1, HIGH);
        }).then(EncoderTickLength / 2, [] {
            digitalWrite(encoder_2, HIGH);
        }).then(100, [&] { update(); });
    }

    void down() {
        digitalWrite(encoder_2, LOW);
        timer.defer(EncoderTickLength / 2, [] {
            digitalWrite(encoder_1, LOW);
        }).then(EncoderTickLength / 2, [] {
            digitalWrite(encoder_2, HIGH);
        }).then(EncoderTickLength / 2, [] {
            digitalWrite(encoder_1, HIGH);
        }).then(100, [&] { update(); });

    }

    void loop(const unsigned long _millis) {
        timer.loop(_millis);
    }

private:
    ScreenBuffer &screenBuffer;
    TimerThing timer{};

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