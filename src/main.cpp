#include "Arduino.h"
#include "hd44780.h"

const byte lcd_pins[] = {25, 26, 32, 33};
const byte lcd_clk = 27;
const byte lcd_rs = 14;

struct lcd_cap {
    volatile bool cmd;
    volatile char data;
};

class CapBuffer {
    const static byte SIZE = 80;
private:
    lcd_cap buffer[SIZE];
    byte cursorPos, readCursorPos;
public:

    void write(lcd_cap current) {
        buffer[this->cursorPos] = current;
        this->cursorPos = static_cast<byte>((this->cursorPos + 1) % SIZE);
    }

    lcd_cap *read() {
        if (this->readCursorPos == this->cursorPos) {
            return nullptr;
        } else {
            this->readCursorPos = static_cast<byte>((this->readCursorPos + 1) % SIZE);
            return &buffer[this->readCursorPos];
        }
    }
};

CapBuffer buffer;

IRAM_ATTR void read() {
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
        buffer.write(current);
        current = lcd_cap();
    }

    firstHalf = !firstHalf;
}

void processItem() {
    auto cap = buffer.read();
    if (cap == nullptr) return;
    if (cap->cmd) {
        if ((cap->data & LCD_SETDDRAMADDR) == LCD_SETDDRAMADDR) {
            auto loc = static_cast<byte>(cap->data & 0x7F); // grab last 7 bits
            auto row = static_cast<byte>(loc / 20), col = static_cast<byte>(loc % 20);
            if (loc == 0) {
                Serial.println();
            }
        }
    } else {
        Serial.write(cap->data);
    }
}

void setup() {
    Serial.begin(115200);
    for (unsigned char lcd_pin : lcd_pins) {
        pinMode(lcd_pin, INPUT_PULLDOWN);
    }
    pinMode(lcd_clk, INPUT_PULLDOWN);
    pinMode(lcd_rs, INPUT_PULLDOWN);
    attachInterrupt(lcd_clk, read, RISING);
}

void loop() {
    processItem();
}