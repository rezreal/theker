#include "relay.h"

namespace hismith {
namespace {

// Rollover-safe deadline compare. millis() wraps every ~49.7 days; comparing
// the signed difference keeps that from latching the relay on across the wrap.
inline bool reached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

}  // namespace

void RelayController::squirt(uint32_t now_ms) {
    if (state_ == State::Lockout) {
        return;
    }
    if (state_ == State::Idle) {
        on_since_ = now_ms;
        enter(State::Holding);
    }
    hold_deadline_ = now_ms + cfg_.hold_ms;
}

void RelayController::tick(uint32_t now_ms) {
    switch (state_) {
        case State::Holding:
            // Checked before the hold deadline so the safety cap wins over a
            // host that keeps extending the hold forever.
            if (reached(now_ms, on_since_ + cfg_.max_on_ms)) {
                lockout_until_ = now_ms + cfg_.cooldown_ms;
                lockout_event_ = true;
                enter(State::Lockout);
            } else if (reached(now_ms, hold_deadline_)) {
                enter(State::Idle);
            }
            break;

        case State::Lockout:
            if (reached(now_ms, lockout_until_)) {
                enter(State::Idle);
            }
            break;

        case State::Idle:
            break;
    }
}

void RelayController::forceOff(uint32_t now_ms) {
    (void)now_ms;
    enter(State::Idle);
}

void RelayController::enter(State s) {
    state_ = s;
    energised_ = (s == State::Holding);
}

bool RelayController::consumeLockoutEvent() {
    const bool event = lockout_event_;
    lockout_event_ = false;
    return event;
}

}  // namespace hismith
