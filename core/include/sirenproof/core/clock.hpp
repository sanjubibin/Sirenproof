#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

#include "sirenproof/core/time.hpp"

namespace sirenproof::core {

using Millis = std::chrono::milliseconds;
using WallTime = std::chrono::sys_time<Millis>;

// One look at the machine's clocks, taken by the platform.
struct ClockReading {
    WallTime wall;          // the Windows clock: anyone with the rights can change it
    Millis monotonic{0};    // time since boot, including sleep; can't be changed, resets on reboot
    std::uint64_t boot_id = 0;  // changes on every boot
};

// What the service saves so trusted time survives restarts and reboots.
struct ClockCheckpoint {
    WallTime trusted;
    WallTime wall;  // the Windows clock when the checkpoint was taken
    Millis monotonic{0};
    std::uint64_t boot_id = 0;

    friend bool operator==(const ClockCheckpoint&, const ClockCheckpoint&) = default;
};

// Decides whether moving evaluation from trusted time `from` to wall time `to` is safe, meaning it
// wouldn't loosen anything that is in force. Supplied by the policy engine.
using TimeChangeArbiter = std::function<bool(TimePoint from, TimePoint to)>;

// Time that changing the Windows clock can't move while it would weaken a block.
//
// - While running, trusted time advances by the monotonic clock.
// - Small differences from the wall clock (time sync, drift) are absorbed slowly: at most one second
//   per minute, so a string of small clock changes can't add up to a bypass.
// - A larger jump is taken over at once only if the arbiter says it loosens nothing. Otherwise it's
//   ignored, and the difference stays as offset() until a later jump brings the clock back, or
//   accepting it becomes safe.
// - Across a reboot the monotonic clock restarts, so trusted time advances by how far the wall clock
//   moved while the PC was off, never backwards. A clock change made *before* the reboot was already
//   seen and doesn't count as time passing.
class TrustedClock {
public:
    static constexpr Millis kJumpTolerance = std::chrono::minutes{2};

    // First start: the wall clock is all there is to go on.
    explicit TrustedClock(const ClockReading& first);
    // Later starts: continue from what was saved.
    TrustedClock(const ClockCheckpoint& saved, const ClockReading& now, const TimeChangeArbiter& arbiter);

    void update(const ClockReading& reading, const TimeChangeArbiter& arbiter);

    TimePoint now() const { return std::chrono::floor<Seconds>(trusted_); }
    // How far the Windows clock is from trusted time (wall - trusted).
    Millis offset() const { return wall_ - trusted_; }
    // True if the last update ignored a clock jump.
    bool last_jump_ignored() const { return last_jump_ignored_; }

    ClockCheckpoint checkpoint() const { return {trusted_, wall_, monotonic_, boot_id_}; }

private:
    WallTime trusted_;
    WallTime wall_;
    Millis monotonic_;
    std::uint64_t boot_id_;
    bool last_jump_ignored_ = false;
};

}  // namespace sirenproof::core
