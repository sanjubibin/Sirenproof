#include "sirenproof/core/schedule.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <map>
#include <utility>

namespace sirenproof::core {

namespace {

constexpr std::size_t kWeek = static_cast<std::size_t>(kMinutesPerWeek);

std::size_t index(int minute_of_week) {
    return static_cast<std::size_t>(((minute_of_week % kMinutesPerWeek) + kMinutesPerWeek) % kMinutesPerWeek);
}

void set_range(WeekMinutes& bits, int day, int from, int to) {
    for (int m = from; m < to; ++m) bits.set(index(day * kMinutesPerDay + m));
}

struct Run {
    int from;
    int to;  // exclusive; below `from` for a run that goes on past midnight
};

}  // namespace

std::optional<WeeklySchedule> WeeklySchedule::from_minutes(const WeekMinutes& minutes) {
    if (minutes.none()) return std::nullopt;

    // Each day's runs of active minutes, in order.
    std::array<std::vector<Run>, 7> days;
    for (int day = 0; day < 7; ++day) {
        const auto on = [&](int m) { return minutes.test(index(day * kMinutesPerDay + m)); };
        for (int m = 0; m < kMinutesPerDay;) {
            if (!on(m)) {
                ++m;
                continue;
            }
            const int start = m;
            while (m < kMinutesPerDay && on(m)) ++m;
            days[static_cast<std::size_t>(day)].push_back({start, m});
        }
    }

    // An evening that reaches midnight and the next morning's first run become one overnight window,
    // as long as the morning ends before the evening starts (a window can't overlap itself).
    for (std::size_t day = 0; day < 7; ++day) {
        auto& today = days[day];
        auto& tomorrow = days[(day + 1) % 7];
        if (today.empty() || tomorrow.empty()) continue;
        Run& evening = today.back();
        const Run morning = tomorrow.front();
        if (evening.to == kMinutesPerDay && evening.from > 0 && morning.from == 0 && morning.to < evening.from) {
            evening.to = morning.to;
            tomorrow.erase(tomorrow.begin());
        }
    }

    // Days with the same hours share a window.
    std::map<std::pair<int, int>, std::uint8_t> shared;
    for (std::size_t day = 0; day < 7; ++day) {
        for (const Run& run : days[day]) {
            auto& mask = shared[{run.from, run.to}];
            mask = static_cast<std::uint8_t>(mask | (1u << day));
        }
    }
    std::vector<WeeklyWindow> windows;
    for (const auto& [hours, mask] : shared) windows.push_back({mask, hours.first, hours.second});
    std::ranges::sort(windows, [](const WeeklyWindow& a, const WeeklyWindow& b) {
        const int first_a = std::countr_zero(static_cast<unsigned>(a.days));
        const int first_b = std::countr_zero(static_cast<unsigned>(b.days));
        return first_a != first_b ? first_a < first_b : a.from < b.from;
    });
    return make(std::move(windows));
}

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
