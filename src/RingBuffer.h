#include "Arduino.h"

template<typename T>
class RingBuffer {
    const static byte SIZE = 80;
private:
    T buffer[SIZE];
    byte cursorPos, readCursorPos;
public:

    void write(T current) {
        buffer[this->cursorPos] = current;
        this->cursorPos = static_cast<byte>((this->cursorPos + 1) % SIZE);
    }

    T *read() {
        if (this->readCursorPos == this->cursorPos) {
            return nullptr;
        } else {
            this->readCursorPos = static_cast<byte>((this->readCursorPos + 1) % SIZE);
            return &buffer[this->readCursorPos];
        }
    }
};
