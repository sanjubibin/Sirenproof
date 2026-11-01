#include "sirenproof/core/clock.hpp"

#include <algorithm>

namespace sirenproof::core {

TrustedClock::TrustedClock(const ClockReading& first)
    : trusted_(first.wall), wall_(first.wall), monotonic_(first.monotonic), boot_id_(first.boot_id) {}

TrustedClock::TrustedClock(const ClockCheckpoint& saved, const ClockReading& now, const TimeChangeArbiter& arbiter)
    : trusted_(saved.trusted), wall_(saved.wall), monotonic_(saved.monotonic), boot_id_(saved.boot_id) {
    update(now, arbiter);
}

void TrustedClock::update(const ClockReading& reading, const TimeChangeArbiter& arbiter) {
    const bool same_boot = reading.boot_id == boot_id_ && reading.monotonic >= monotonic_;
    const Millis elapsed = same_boot ? reading.monotonic - monotonic_ : std::max(Millis{0}, reading.wall - wall_);
    trusted_ += elapsed;

    const Millis difference = reading.wall - trusted_;
    last_jump_ignored_ = false;
    if (std::chrono::abs(difference) <= kJumpTolerance) {
        const Millis max_step = elapsed / 60;
        trusted_ += std::clamp(difference, -max_step, max_step);
    } else if (arbiter(std::chrono::floor<Seconds>(trusted_), std::chrono::floor<Seconds>(reading.wall))) {
        trusted_ = reading.wall;
    } else {
        last_jump_ignored_ = true;
    }

    wall_ = reading.wall;
    monotonic_ = reading.monotonic;
    boot_id_ = reading.boot_id;
}

}  // namespace sirenproof::core
