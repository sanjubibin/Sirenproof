#include <catch2/catch_test_macros.hpp>

#include "sirenproof/core/time.hpp"
#include "test_support.hpp"

using namespace sirenproof::core;
using sirenproof::test::at;

TEST_CASE("minute_of_week counts from Monday 00:00 local time", "[time]") {
    CHECK(minute_of_week(at("2026-11-02T00:00:00Z"), Minutes{0}) == 0);  // a Monday
    CHECK(minute_of_week(at("2026-11-02T09:30:00Z"), Minutes{0}) == 570);
    CHECK(minute_of_week(at("2026-11-08T23:59:00Z"), Minutes{0}) == kMinutesPerWeek - 1);  // Sunday

    // India (+05:30): Sunday 18:30 UTC is already Monday 00:00 locally.
    CHECK(minute_of_week(at("2026-11-01T18:30:00Z"), Minutes{330}) == 0);
    // New York (-05:00): Monday 03:00 UTC is still Sunday 22:00 locally.
    CHECK(minute_of_week(at("2026-11-02T03:00:00Z"), Minutes{-300}) == 6 * kMinutesPerDay + 22 * 60);
}

TEST_CASE("local_date uses the local calendar day", "[time]") {
    CHECK(local_date(at("2026-11-01T20:00:00Z"), Minutes{0}) == "2026-11-01");
    CHECK(local_date(at("2026-11-01T20:00:00Z"), Minutes{330}) == "2026-11-02");
}

TEST_CASE("format_time and parse_time round-trip", "[time]") {
    const TimePoint t = at("2026-11-02T09:30:15Z");
    CHECK(format_time(t) == "2026-11-02T09:30:15Z");
    CHECK(parse_time(format_time(t)) == t);
}

TEST_CASE("parse_time accepts only the strict UTC form", "[time]") {
    CHECK_FALSE(parse_time("2026-11-02 09:30:00Z"));
    CHECK_FALSE(parse_time("2026-11-02T09:30:00"));
    CHECK_FALSE(parse_time("2026-02-30T00:00:00Z"));
    CHECK_FALSE(parse_time("2026-11-02T24:00:00Z"));
    CHECK_FALSE(parse_time("2026-11-02T09:60:00Z"));
    CHECK_FALSE(parse_time("tomorrow"));
}

TEST_CASE("clock times are HH:MM with 24:00 allowed as an end", "[time]") {
    CHECK(parse_clock("09:00") == 540);
    CHECK(parse_clock("00:00") == 0);
    CHECK(parse_clock("24:00") == kMinutesPerDay);
    CHECK_FALSE(parse_clock("24:01"));
    CHECK_FALSE(parse_clock("9:00"));
    CHECK_FALSE(parse_clock("12:60"));
    CHECK(format_clock(540) == "09:00");
    CHECK(format_clock(kMinutesPerDay) == "24:00");
}

TEST_CASE("parse_duration reads lengths as people type them", "[time]") {
    CHECK(parse_duration("25m") == Minutes{25});
    CHECK(parse_duration("2h") == Minutes{120});
    CHECK(parse_duration("1h30m") == Minutes{90});
    CHECK(parse_duration("90") == Minutes{90});
    CHECK_FALSE(parse_duration(""));
    CHECK_FALSE(parse_duration("0m"));
    CHECK_FALSE(parse_duration("h"));
    CHECK_FALSE(parse_duration("2 h"));
    CHECK_FALSE(parse_duration("reddit.com"));
    CHECK_FALSE(parse_duration("9999999m"));
}
