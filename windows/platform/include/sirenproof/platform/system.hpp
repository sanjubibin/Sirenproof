#pragma once

#include <cstdint>
#include <span>

#include "sirenproof/core/clock.hpp"
#include "sirenproof/core/crypto.hpp"
#include "sirenproof/core/time.hpp"

namespace sirenproof::platform {

// The machine's clocks right now: the Windows clock, time since boot (counting sleep), and the boot id.
core::ClockReading read_clock();

// Local time's offset from UTC at `t` under the current Windows time zone, daylight saving included.
core::Minutes local_offset_at(core::TimePoint t);

// BCryptGenRandom, the system's secure random number generator.
class SystemRandom : public core::RandomSource {
public:
    void fill(std::span<std::uint8_t> out) override;
};

}  // namespace sirenproof::platform
