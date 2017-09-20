#pragma once

#include <bits/stdc++.h>
#include <esp32-hal-log.h>

enum class MenuTaskState {
    Incomplete,
    Complete,
    Retry,
    Abort
};

class MenuManager;

class MenuTask {
    uint16_t timeoutStart;
    unsigned long timeoutTime = 0;
    uint8_t retries = 0;
    std::function<void(bool)> callback;
protected:
    MenuTaskState currentState = MenuTaskState::Incomplete;
public:

    void executeCallback();

    explicit MenuTask(uint16_t timeout, std::function<void(bool)> callback)
            : timeoutStart(timeout), callback(std::move(callback)) {}

    virtual void Run(MenuManager &mm) = 0;

    virtual MenuTaskState IsComplete(MenuManager &mm, unsigned long currentTime) {
        //log_v("timeoutTime = %u, retries = %u, currentTime = %u", timeoutTime, retries, currentTime);

        if (timeoutTime == 0) {
            log_v("starting timeout timer");
            timeoutTime = currentTime + timeoutStart;
        }

        if (currentTime > timeoutTime) {
            if (retries < 3) {
                retries++;
                timeoutTime = currentTime + timeoutStart;
                log_v("retries: %u", retries);
                return (currentState = MenuTaskState::Retry);
            } else {
                log_v("aborting");
                return (currentState = MenuTaskState::Abort);
            }
        }

        return (currentState = MenuTaskState::Incomplete);
    }
};


class MoveTask : public MenuTask {
    int8_t direction;

public:
    explicit MoveTask(int8_t direction, const std::function<void(bool)> &callback);

    void Run(MenuManager &mm) override;

    MenuTaskState IsComplete(MenuManager &mm, unsigned long currentTime) override;
};

class ClickTask : public MenuTask {
public:
    explicit ClickTask(const std::function<void(bool)> &callback);

    MenuTaskState IsComplete(MenuManager &mm, unsigned long currentTime) override;

    void Run(MenuManager &mm) override;
};

class FindTask : public MenuTask {
    std::string toFind;

public:
    explicit FindTask(std::string toFind, const std::function<void(bool)> &callback);

    void Run(MenuManager &mm) override;

    MenuTaskState IsComplete(MenuManager &mm, unsigned long currentTime) override;
};
