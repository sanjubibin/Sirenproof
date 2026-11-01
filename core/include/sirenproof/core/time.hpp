#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace sirenproof::core {

// Instants are whole UTC seconds. Every time stored in rules is "trusted time" (see clock.hpp), which a
// user can't move by changing the Windows clock.
using TimePoint = std::chrono::sys_seconds;
using Seconds = std::chrono::seconds;
using Minutes = std::chrono::minutes;
using Hours = std::chrono::hours;

// The local offset from UTC at a given instant, including daylight saving. Supplied by the platform.
using LocalOffsetFn = std::function<Minutes(TimePoint)>;

inline constexpr int kMinutesPerDay = 24 * 60;
inline constexpr int kMinutesPerWeek = 7 * kMinutesPerDay;

// Position in the local week: 0 is Monday 00:00, kMinutesPerWeek - 1 is Sunday 23:59.
int minute_of_week(TimePoint t, Minutes local_offset);

// Local calendar date, "YYYY-MM-DD".
std::string local_date(TimePoint t, Minutes local_offset);

// "2026-11-02T09:30:00Z". parse_time accepts exactly this form.
std::string format_time(TimePoint t);
std::optional<TimePoint> parse_time(std::string_view text);

// "HH:MM" minutes since midnight, 0-1440 ("24:00" is allowed as an end time).
std::string format_clock(int minutes);
std::optional<int> parse_clock(std::string_view text);

}  // namespace sirenproof::core
