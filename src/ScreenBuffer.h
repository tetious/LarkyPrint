#pragma once

#include "Arduino.h"
#include "hd44780.h"
#include <array>
#include <algorithm>

using namespace std;

struct lcd_cap {
    bool cmd;
    char data;
};

class ScreenBuffer {
public:
    typedef function<void(ScreenBuffer *, bool)> bufferUpdateCallback;
private:
    const static byte SIZE = 80;
    const static uint8_t updateTimeoutMs = 5;
    array<char, SIZE> buffer{};
    byte cursorPos = 0;
    byte leftToSkip = 0;
    unsigned long lastUpdate = 0;
    bool updateSent = true;
    bool screenCleared = false;
    vector<bufferUpdateCallback> updateCallbacks{};

    void updateTimeout() {
        if (!updateSent && millis() - lastUpdate > updateTimeoutMs) {
            for (auto cb: updateCallbacks) {
                cb(this, screenCleared);
            }
            screenCleared = false;
            updateSent = true;
        }
    }

public:
    ScreenBuffer() {
        buffer.fill(' ');
        xTaskCreate([](void *o) {
            TickType_t lastWakeTime;
            const auto freq = updateTimeoutMs / portTICK_PERIOD_MS;
            while (true) {
                lastWakeTime = xTaskGetTickCount();
                static_cast<ScreenBuffer *>(o)->updateTimeout();
                vTaskDelayUntil(&lastWakeTime, freq);
            }
        }, "sb_loop", 2048, this, 1, nullptr);
    }

    void subUpdate(bufferUpdateCallback cb) {
        updateCallbacks.push_back(cb);
    }

    IRAM_ATTR void write(lcd_cap &cap) {

        lastUpdate = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

        if (leftToSkip > 0) {
            leftToSkip--;
            return;
        }

        if (cap.cmd) {
            if ((cap.data & LCD_SETDDRAMADDR) == LCD_SETDDRAMADDR) {
                auto dramLoc = static_cast<byte>(cap.data & 0x7F); // grab last 7 bits
                byte loc = dramLoc;
                if (dramLoc >= 20 && dramLoc < 40) { // line 3
                    loc += 20;
                } else if (dramLoc >= 64 && dramLoc < 84) { // line 2
                    loc -= 44;
                } else if (dramLoc >= 84) { // line 4
                    loc -= 24;
                }
                auto row = static_cast<byte>(loc / 20), col = static_cast<byte>(loc % 20);
                cursorPos = loc;
            } else if ((cap.data & LCD_CLEARDISPLAY) == LCD_CLEARDISPLAY) {
                buffer.fill(' ');
                screenCleared = true;
                cursorPos = 0;
            } else if ((cap.data & LCD_SETCGRAMADDR) == LCD_SETCGRAMADDR) {
                // next 8 bytes are graphical data and should be skipped.
                leftToSkip = 8;
            }
            return;
        }
        if (cursorPos > SIZE) {
            Serial.printf("CursorPos is too high!! %u", cursorPos);
            return;
        }
        buffer[cursorPos] = cap.data;
        cursorPos++;
        updateSent = false;
    }

    array<char, SIZE> read() {
        return buffer;
    }
};