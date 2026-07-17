#pragma once

#include <cstdint>

// Relay control logic.
//
// Pure C++: the caller supplies the clock and applies the output level to a
// GPIO, so this builds and is tested on the host.

namespace hismith {

// "Hold while repeating": a squirt command energises the relay and each further
// command extends the hold. The relay releases once the commands stop, so the
// host controls dose by how long it keeps sending.
//
// Because this drives a pump, the state machine owns two failsafes: a hard cap
// on continuous energised time (Lockout), and forceOff() for BLE disconnect.
class RelayController {
  public:
    enum class State : uint8_t {
        Idle,     // de-energised, ready
        Holding,  // energised, releases at hold_deadline_
        Lockout,  // de-energised after tripping max_on_ms; squirts ignored
    };

    struct Config {
        uint32_t hold_ms;
        uint32_t max_on_ms;
        uint32_t cooldown_ms;
    };

    explicit RelayController(const Config& cfg) : cfg_(cfg) {}

    // A valid squirt command arrived. Starts or extends the hold. Ignored
    // during Lockout.
    void squirt(uint32_t now_ms);

    // Advances the state machine. Call every loop iteration.
    void tick(uint32_t now_ms);

    // Failsafe: de-energise immediately and clear any hold.
    void forceOff(uint32_t now_ms);

    bool energised() const { return energised_; }
    State state() const { return state_; }

    // True once per Lockout entry, for logging.
    bool consumeLockoutEvent();

  private:
    void enter(State s);

    Config cfg_;
    State state_ = State::Idle;
    bool energised_ = false;
    uint32_t hold_deadline_ = 0;
    uint32_t on_since_ = 0;
    uint32_t lockout_until_ = 0;
    bool lockout_event_ = false;
};

}  // namespace hismith
