#pragma once

#include "pins.h"
#include "bits/stdc++.h"

const uint8_t TEMP_SYMBOL = 0x02, UP_ARROW = 0x03;
const int8_t NO_MOVE = -99;

class ScreenBuffer;
class TimerThing;
class MenuTask;

class MenuManager {
    const uint8_t EncoderTickLength = 20;
    const uint8_t EncoderMoveTime = EncoderTickLength / 2 * 3;
    const int8_t mnu_SdCard = 3;
    const int8_t mnu_Control = 2;
    const std::list<int8_t> mnu_Temp = {mnu_Control, 1};

    void _down(const std::function<void()> &done = nullptr);

    void _up(const std::function<void()> &done = nullptr);

public:
    explicit MenuManager(ScreenBuffer &screenBuffer);

    // todo: rework so up and down are reliable even with buggy encoder config

    void click();

    void up();

    void down();

    void moveTo(std::list<int8_t> path, const std::function<void(bool)> &cb);

    void find(const std::string &item, std::function<void(bool)> cb, bool recursing);

    void printFile(std::string filename, const std::function<void(bool)> &cb);

    void setTemp(std::string heater, uint8_t temp, std::function<void(bool)> cb);

    int8_t hasMoved();

    bool hasClicked();

private:
    ScreenBuffer &screenBuffer;
    TimerThing &timer;
    bool menuEncoderReversed = true;
    bool _hasMoved = false;
    int8_t moved = 0;
    std::queue<MenuTask*> taskQueue{};

    uint8_t menuLevel = 0;
    bool clickGoesUp = false;
    std::string menuItem = "";

    void attachUpdate();

    void processQueue(unsigned long millis);

    bool _hasClicked;
};
