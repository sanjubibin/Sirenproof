#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "sirenproof/core/schedule.hpp"
#include "test_support.hpp"

using namespace sirenproof::core;
using sirenproof::test::schedule;

namespace {

constexpr std::uint8_t kMonday = 1u << 0;
constexpr std::uint8_t kFriday = 1u << 4;
constexpr std::uint8_t kSaturday = 1u << 5;
constexpr std::uint8_t kSunday = 1u << 6;

constexpr int day(int index, int hour, int minute = 0) { return index * kMinutesPerDay + hour * 60 + minute; }

}  // namespace

TEST_CASE("WeeklySchedule::make rejects malformed windows", "[schedule]") {
    CHECK_FALSE(WeeklySchedule::make({}));
    CHECK_FALSE(WeeklySchedule::make({{0, 540, 1020}}));           // no days
    CHECK_FALSE(WeeklySchedule::make({{128, 540, 1020}}));         // an eighth day
    CHECK_FALSE(WeeklySchedule::make({{kWeekdays, 540, 540}}));    // empty
    CHECK_FALSE(WeeklySchedule::make({{kWeekdays, 1440, 60}}));    // starts at 24:00
    CHECK_FALSE(WeeklySchedule::make({{kWeekdays, 540, 0}}));      // ends at 00:00 means nothing
    CHECK_FALSE(WeeklySchedule::make({{kWeekdays, -1, 60}}));
    CHECK(WeeklySchedule::make({{kWeekdays, 0, 1440}}));
}

TEST_CASE("A daytime window is active from its start up to, not including, its end", "[schedule]") {
    const WeeklySchedule work = schedule(kWeekdays, 9 * 60, 17 * 60);
    CHECK(work.active_at(day(0, 9)));
    CHECK(work.active_at(day(4, 16, 59)));
    CHECK_FALSE(work.active_at(day(0, 8, 59)));
    CHECK_FALSE(work.active_at(day(0, 17)));
    CHECK_FALSE(work.active_at(day(5, 10)));  // Saturday
}

TEST_CASE("An overnight window runs into the next day, and Sunday wraps to Monday", "[schedule]") {
    const WeeklySchedule late = schedule(kSunday, 22 * 60, 2 * 60);
    CHECK(late.active_at(day(6, 23)));
    CHECK(late.active_at(day(0, 1, 59)));
    CHECK_FALSE(late.active_at(day(0, 2)));
    CHECK_FALSE(late.active_at(day(5, 23)));
}

TEST_CASE("contains compares schedules minute by minute", "[schedule]") {
    const WeeklySchedule work = schedule(kWeekdays, 9 * 60, 17 * 60);
    const WeeklySchedule monday_morning = schedule(kMonday, 10 * 60, 12 * 60);
    const WeeklySchedule longer_days = schedule(kWeekdays, 8 * 60, 18 * 60);

    CHECK(work.contains(monday_morning));
    CHECK_FALSE(monday_morning.contains(work));
    CHECK(longer_days.contains(work));
    CHECK(work.contains(work));
}

TEST_CASE("minutes_until_change finds the next switch", "[schedule]") {
    const WeeklySchedule work = schedule(kWeekdays, 9 * 60, 17 * 60);
    CHECK(work.minutes_until_change(day(0, 16)) == 60);   // switches off at 17:00
    CHECK(work.minutes_until_change(day(0, 8)) == 60);    // switches on at 09:00
    CHECK(work.minutes_until_change(day(4, 18)) == 2 * kMinutesPerDay + 15 * 60);  // Friday evening to Monday

    CHECK(schedule(kEveryDay, 0, kMinutesPerDay).minutes_until_change(0) == kMinutesPerWeek);
}

TEST_CASE("longest_active_run measures the longest unbroken stretch, across the week's end", "[schedule]") {
    CHECK(schedule(kWeekdays, 9 * 60, 17 * 60).longest_active_run() == 8 * 60);
    CHECK(schedule(kEveryDay, 22 * 60, 6 * 60).longest_active_run() == 8 * 60);
    CHECK(schedule(kEveryDay, 0, kMinutesPerDay).longest_active_run() == kMinutesPerWeek);

    // Friday 22:00 through the weekend to Monday 02:00: 52 hours, wrapping from Sunday into Monday.
    const auto weekend = WeeklySchedule::make({{kFriday, 22 * 60, kMinutesPerDay},
                                               {static_cast<std::uint8_t>(kSaturday | kSunday), 0, kMinutesPerDay},
                                               {kMonday, 0, 2 * 60}});
    REQUIRE(weekend);
    CHECK(weekend->longest_active_run() == 52 * 60);
}

TEST_CASE("from_minutes writes the same minutes back as few, readable windows", "[schedule]") {
    CHECK_FALSE(WeeklySchedule::from_minutes({}));

    // Office hours on weekdays come back as one window.
    const auto work = WeeklySchedule::from_minutes(schedule(kWeekdays, 9 * 60, 17 * 60).minutes());
    REQUIRE(work);
    CHECK(work->windows() == std::vector<WeeklyWindow>{{kWeekdays, 9 * 60, 17 * 60}});

    // Evenings that run past midnight come back as one overnight window, Sunday into Monday included.
    const auto nights = WeeklySchedule::from_minutes(schedule(kEveryDay, 22 * 60, 6 * 60).minutes());
    REQUIRE(nights);
    CHECK(nights->windows() == std::vector<WeeklyWindow>{{kEveryDay, 22 * 60, 6 * 60}});

    // Whole days stay whole days rather than joining up into one long overnight window.
    const auto weekend = WeeklySchedule::from_minutes(
        schedule(static_cast<std::uint8_t>(kSaturday | kSunday), 0, kMinutesPerDay).minutes());
    REQUIRE(weekend);
    CHECK(weekend->windows() ==
          std::vector<WeeklyWindow>{{static_cast<std::uint8_t>(kSaturday | kSunday), 0, kMinutesPerDay}});
}

TEST_CASE("from_minutes keeps every minute exactly, however the hours were painted", "[schedule]") {
    // A messy week: office hours from 09:30, a lunch gap on Tuesday, odd minutes on Wednesday, a late
    // Friday that runs into Saturday, and a Sunday evening that can't join Monday's long morning
    // because that morning runs past 20:00.
    const auto set = [](WeekMinutes& bits, int from, int to, bool on) {
        for (int m = from; m < to; ++m) bits.set(static_cast<std::size_t>(m), on);
    };
    WeekMinutes minutes = schedule(kWeekdays, 9 * 60 + 30, 17 * 60).minutes();
    set(minutes, day(1, 12), day(1, 13), false);
    set(minutes, day(2, 20, 7), day(2, 20, 52), true);
    set(minutes, day(4, 23), day(5, 3), true);
    set(minutes, day(6, 20), day(6, 24), true);
    set(minutes, day(0, 0), day(0, 21), true);

    const auto rebuilt = WeeklySchedule::from_minutes(minutes);
    REQUIRE(rebuilt);
    CHECK(rebuilt->minutes() == minutes);

    const auto has = [&](WeeklyWindow w) { return std::ranges::count(rebuilt->windows(), w) == 1; };
    constexpr std::uint8_t kWednesdayToFriday = 0b0011100;
    CHECK(has({kWednesdayToFriday, 9 * 60 + 30, 17 * 60}));
    CHECK(has({kFriday, 23 * 60, 3 * 60}));
    CHECK(has({kSunday, 20 * 60, kMinutesPerDay}));
    CHECK(has({kMonday, 0, 21 * 60}));
}
