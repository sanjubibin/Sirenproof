#include <catch2/catch_test_macros.hpp>

#include "sirenproof/core/clock.hpp"
#include "test_support.hpp"

using namespace sirenproof::core;
using sirenproof::test::at;
using std::chrono::hours;
using std::chrono::seconds;

namespace {

const TimeChangeArbiter kAllowAll = [](TimePoint, TimePoint) { return true; };
const TimeChangeArbiter kDenyAll = [](TimePoint, TimePoint) { return false; };

const WallTime kStart = at("2026-11-02T09:00:00Z");

ClockReading reading(WallTime wall, Millis monotonic, std::uint64_t boot = 1) { return {wall, monotonic, boot}; }

}  // namespace

TEST_CASE("Trusted time follows the monotonic clock while nothing is tampered with", "[clock]") {
    TrustedClock clock(reading(kStart, Millis{0}));
    clock.update(reading(kStart + seconds{10}, seconds{10}), kDenyAll);
    CHECK(clock.now() == at("2026-11-02T09:00:10Z"));
    CHECK(clock.offset() == Millis{0});
    CHECK_FALSE(clock.last_jump_ignored());

    // Many small steps lose nothing to rounding.
    for (int i = 1; i <= 100; ++i) clock.update(reading(kStart + seconds{10} + Millis{250 * i}, Millis{10'000 + 250 * i}), kDenyAll);
    CHECK(clock.now() == at("2026-11-02T09:00:35Z"));
}

TEST_CASE("A clock jump is ignored when accepting it would loosen something", "[clock][tamper]") {
    TrustedClock clock(reading(kStart, Millis{0}));
    clock.update(reading(kStart + hours{3} + seconds{20}, seconds{20}), kDenyAll);  // moved 3 hours ahead
    CHECK(clock.now() == at("2026-11-02T09:00:20Z"));
    CHECK(clock.offset() == hours{3});
    CHECK(clock.last_jump_ignored());

    clock.update(reading(kStart - hours{5}, seconds{30}), kDenyAll);  // and backwards
    CHECK(clock.now() == at("2026-11-02T09:00:30Z"));
}

TEST_CASE("A clock jump is taken over when it loosens nothing", "[clock]") {
    TrustedClock clock(reading(kStart, Millis{0}));
    clock.update(reading(kStart + hours{3}, seconds{1}), kAllowAll);
    CHECK(clock.now() == at("2026-11-02T12:00:00Z"));
    CHECK(clock.offset() == Millis{0});
}

TEST_CASE("Small differences are absorbed slowly, at most a second per minute", "[clock]") {
    TrustedClock clock(reading(kStart, Millis{0}));
    // The wall clock is corrected 60 seconds ahead (within tolerance) while a minute passes.
    clock.update(reading(kStart + seconds{120}, seconds{60}), kDenyAll);
    CHECK(clock.now() == at("2026-11-02T09:01:01Z"));
    CHECK_FALSE(clock.last_jump_ignored());
}

TEST_CASE("Many small clock nudges can't add up to a bypass", "[clock][tamper]") {
    TrustedClock clock(reading(kStart, Millis{0}));
    WallTime wall = kStart;
    for (int i = 1; i <= 100; ++i) {
        wall += seconds{1} + seconds{100};  // a second passes, and the clock is pushed 100 s ahead each time
        clock.update(reading(wall, seconds{i}), kDenyAll);
    }
    // 100 real seconds passed; slewing may add at most 1/60 of that.
    CHECK(clock.now() >= at("2026-11-02T09:01:40Z"));
    CHECK(clock.now() <= at("2026-11-02T09:01:42Z"));
}

TEST_CASE("Across a reboot, the time the PC was off counts when that loosens nothing", "[clock]") {
    const ClockCheckpoint saved{kStart, kStart, Millis{5'000}, 1};
    TrustedClock clock(saved, reading(kStart + hours{3}, Millis{1'000}, 2), kAllowAll);
    CHECK(clock.now() == at("2026-11-02T12:00:00Z"));
    CHECK(clock.offset() == Millis{0});
}

TEST_CASE("Setting the clock forward while the PC is off doesn't end a lock", "[clock][tamper]") {
    // Off for an unknown time, with the firmware clock moved 3 hours on; up again for 10 seconds.
    const ClockCheckpoint saved{kStart, kStart, Millis{5'000}, 1};
    TrustedClock clock(saved, reading(kStart + hours{3}, seconds{10}, 2), kDenyAll);
    CHECK(clock.now() == at("2026-11-02T09:00:10Z"));  // only the time since boot counts
    CHECK(clock.last_jump_ignored());

    // Once counting it is safe (the lock would have ended anyway), the clock catches up.
    clock.update(reading(kStart + hours{3} + seconds{11}, seconds{11}), kAllowAll);
    CHECK(clock.now() == at("2026-11-02T12:00:11Z"));
}

TEST_CASE("A clock change made before a reboot doesn't count as time passing", "[clock][tamper]") {
    TrustedClock clock(reading(kStart, Millis{0}));
    clock.update(reading(kStart + hours{1}, Millis{0}), kDenyAll);  // moved 1 hour ahead: ignored
    const ClockCheckpoint saved = clock.checkpoint();

    // Off for two hours, then booted again. Counting the time off is safe; jumping on to the clock isn't.
    const TimeChangeArbiter until_11 = [](TimePoint, TimePoint to) { return to <= at("2026-11-02T11:00:00Z"); };
    TrustedClock after(saved, reading(kStart + hours{3}, Millis{500}, 2), until_11);
    CHECK(after.now() == at("2026-11-02T11:00:00Z"));
    CHECK(after.offset() == hours{1});
}

TEST_CASE("Moving the clock back while the PC is off never moves trusted time back", "[clock][tamper]") {
    const ClockCheckpoint saved{kStart, kStart, Millis{5'000}, 1};
    TrustedClock clock(saved, reading(kStart - hours{6}, Millis{1'000}, 2), kDenyAll);
    CHECK(clock.now() == at("2026-11-02T09:00:01Z"));  // just the second since boot
}

TEST_CASE("A service restart without a reboot uses the monotonic clock", "[clock]") {
    const ClockCheckpoint saved{kStart, kStart, Millis{5'000}, 7};
    TrustedClock clock(saved, reading(kStart + seconds{60}, Millis{65'000}, 7), kDenyAll);
    CHECK(clock.now() == at("2026-11-02T09:01:00Z"));
}

TEST_CASE("A checkpoint captures the whole clock", "[clock]") {
    TrustedClock clock(reading(kStart, Millis{1'000}, 3));
    clock.update(reading(kStart + seconds{5}, Millis{6'000}, 3), kDenyAll);
    const ClockCheckpoint c = clock.checkpoint();
    CHECK(c.trusted == kStart + seconds{5});
    CHECK(c.wall == kStart + seconds{5});
    CHECK(c.monotonic == Millis{6'000});
    CHECK(c.boot_id == 3);
}
