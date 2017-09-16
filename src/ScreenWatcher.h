#pragma once

#include "Arduino.h"

namespace ScreenWatcher {

    ScreenBuffer screenBuffer{};

    IRAM_ATTR void _readScreen() {
        static bool firstHalf = true;
        static lcd_cap current = lcd_cap();

        if (digitalRead(lcd_rs) == 0) {
            current.cmd = true;
        }

        // full byte is broken into two, so stuff them into the correct place
        for (int pin = 0; pin < sizeof(lcd_pins); pin++) {
            current.data |= (digitalRead(lcd_pins[pin]) << pin + (firstHalf ? 4 : 0));
        }

        if (!firstHalf) {
            screenBuffer.write(current);
            current = lcd_cap();
        }

        firstHalf = !firstHalf;
    }

    void start() {
        attachInterrupt(lcd_clk, _readScreen, FALLING);
    }

    void stop() {
        detachInterrupt(lcd_clk);
    }

}