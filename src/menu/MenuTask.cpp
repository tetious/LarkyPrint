#include "MenuTask.h"
#include "MenuManager.h"

using namespace std;

void MenuTask::executeCallback() {
    log_v("Complete after %u ms.", (millis() + timeoutStart - timeoutTime) + (timeoutStart * retries));
    log_v("m: %u, tt: %u, ts: %u, r: %u.", millis(), timeoutTime, timeoutStart, retries);
    if (callback) { callback(currentState == MenuTaskState::Complete); }
}

MoveTask::MoveTask(int8_t direction, const function<void(bool)> &callback) :
        MenuTask(500, callback), direction(direction) {}

void MoveTask::Run(MenuManager &mm) {
    if (direction > 0) {
        log_v("moving down");
        mm.down();
    } else {
        log_v("moving up");
        mm.up();
    }
}

MenuTaskState MoveTask::IsComplete(MenuManager &mm, unsigned long currentTime) {
    auto state = MenuTask::IsComplete(mm, currentTime);
    if (state != MenuTaskState::Incomplete) { return state; }

    return (currentState = mm.hasMoved() != NO_MOVE ? MenuTaskState::Complete : MenuTaskState::Incomplete);
}

ClickTask::ClickTask(const function<void(bool)> &callback) :
        MenuTask(500, callback) {}

void ClickTask::Run(MenuManager &mm) {
    log_v("clicking");
    mm.click();
}

MenuTaskState ClickTask::IsComplete(MenuManager &mm, unsigned long currentTime) {
    auto state = MenuTask::IsComplete(mm, currentTime);
    if (state != MenuTaskState::Incomplete) { return state; }

    return (currentState = mm.hasClicked() ? MenuTaskState::Complete : MenuTaskState::Incomplete);
}

FindTask::FindTask(std::string toFind, const std::function<void(bool)> &callback) :
        MenuTask(1000, callback), toFind(std::move(toFind)) {}

void FindTask::Run(MenuManager &mm) {

}

MenuTaskState FindTask::IsComplete(MenuManager &mm, unsigned long currentTime) {
    return MenuTask::IsComplete(mm, currentTime);
}
