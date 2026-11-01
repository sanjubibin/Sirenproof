#include "sirenproof/core/schedule.hpp"

#include <algorithm>

namespace sirenproof::core {

namespace {

constexpr std::size_t kWeek = static_cast<std::size_t>(kMinutesPerWeek);

std::size_t index(int minute_of_week) {
    return static_cast<std::size_t>(((minute_of_week % kMinutesPerWeek) + kMinutesPerWeek) % kMinutesPerWeek);
}

void set_range(std::bitset<kMinutesPerWeek>& bits, int day, int from, int to) {
    for (int m = from; m < to; ++m) bits.set(index(day * kMinutesPerDay + m));
}

}  // namespace

std::optional<WeeklySchedule> WeeklySchedule::make(std::vector<WeeklyWindow> windows) {
    if (windows.empty()) return std::nullopt;

    WeeklySchedule schedule;
    for (const WeeklyWindow& w : windows) {
        if (w.days == 0 || w.days > kEveryDay) return std::nullopt;
        if (w.from < 0 || w.from >= kMinutesPerDay || w.to <= 0 || w.to > kMinutesPerDay || w.from == w.to) {
            return std::nullopt;
        }
        const unsigned days = w.days;
        for (int day = 0; day < 7; ++day) {
            if (((days >> day) & 1u) == 0) continue;
            if (w.from < w.to) {
                set_range(schedule.minutes_, day, w.from, w.to);
            } else {  // overnight: the rest of this day, then the start of the next (Sunday wraps to Monday)
                set_range(schedule.minutes_, day, w.from, kMinutesPerDay);
                set_range(schedule.minutes_, (day + 1) % 7, 0, w.to);
            }
        }
    }
    schedule.windows_ = std::move(windows);
    return schedule;
}

bool WeeklySchedule::active_at(int minute_of_week) const { return minutes_.test(index(minute_of_week)); }

bool WeeklySchedule::contains(const WeeklySchedule& other) const { return (other.minutes_ & ~minutes_).none(); }

int WeeklySchedule::minutes_until_change(int minute_of_week) const {
    const bool now = active_at(minute_of_week);
    for (int step = 1; step < kMinutesPerWeek; ++step) {
        if (active_at(minute_of_week + step) != now) return step;
    }
    return kMinutesPerWeek;
}

int WeeklySchedule::longest_active_run() const {
    if (minutes_.all()) return kMinutesPerWeek;
    if (minutes_.none()) return 0;

    // Start just after an inactive minute so a run that wraps around the week is counted whole.
    std::size_t start = 0;
    while (minutes_.test(start)) ++start;

    int longest = 0;
    int run = 0;
    for (std::size_t step = 1; step <= kWeek; ++step) {
        if (minutes_.test((start + step) % kWeek)) {
            longest = std::max(longest, ++run);
        } else {
            run = 0;
        }
    }
    return longest;
}

}  // namespace sirenproof::core
