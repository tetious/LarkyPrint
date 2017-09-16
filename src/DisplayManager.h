#include "ScreenBuffer.h"

class DisplayManager {
public:  
    explicit DisplayManager(ScreenBuffer &screenBuffer) : screenBuffer(screenBuffer) {}



private:
    ScreenBuffer screenBuffer;
};

