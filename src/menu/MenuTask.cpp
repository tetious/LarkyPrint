#include "MenuTask.h"
#include "MenuManager.h"

using namespace std;

MoveTask::MoveTask(const function<void(bool)> &callback) :
        MenuTask(100, callback) {}

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

    return mm.hasMoved() != NO_MOVE ? MenuTaskState::Complete : MenuTaskState::Incomplete;
}

ClickTask::ClickTask(const function<void(bool)> &callback) :
        MenuTask(1000, callback) {}

void ClickTask::Run(MenuManager &mm) {
    log_v("clicking");
    mm.click();
}

MenuTaskState ClickTask::IsComplete(MenuManager &mm, unsigned long currentTime) {
    auto state = MenuTask::IsComplete(mm, currentTime);
    if (state != MenuTaskState::Incomplete) { return state; }

    return mm.hasClicked() != NO_MOVE ? MenuTaskState::Complete : MenuTaskState::Incomplete;
}