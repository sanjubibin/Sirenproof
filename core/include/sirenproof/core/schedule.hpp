#pragma once

#include <bitset>
#include <cstdint>
#include <optional>
#include <vector>

#include "sirenproof/core/time.hpp"

namespace sirenproof::core {

// A recurring stretch of local time on chosen days, e.g. Mon-Fri 09:00-17:00.
// `from` and `to` are minutes since midnight. If `to` is earlier than `from` the window runs overnight
// into the next day (22:00-02:00). `to` may be 1440 ("24:00").
struct WeeklyWindow {
    std::uint8_t days = 0;  // bit 0 = Monday ... bit 6 = Sunday
    int from = 0;
    int to = 0;

    friend bool operator==(const WeeklyWindow&, const WeeklyWindow&) = default;
};

inline constexpr std::uint8_t kWeekdays = 0b0011111;
inline constexpr std::uint8_t kEveryDay = 0b1111111;

// A set of windows, kept as one bit per minute of the week so that comparisons are exact.
class WeeklySchedule {
public:
    // nullopt if there are no windows or any window is malformed.
    static std::optional<WeeklySchedule> make(std::vector<WeeklyWindow> windows);

    const std::vector<WeeklyWindow>& windows() const noexcept { return windows_; }

    bool active_at(int minute_of_week) const;

    // True if every minute active in `other` is also active here.
    bool contains(const WeeklySchedule& other) const;

    // Minutes from `minute_of_week` until the schedule switches on or off; kMinutesPerWeek if never.
    int minutes_until_change(int minute_of_week) const;

    // The longest stretch of continuously active minutes, wrapping from Sunday into Monday.
    int longest_active_run() const;

    friend bool operator==(const WeeklySchedule& a, const WeeklySchedule& b) { return a.minutes_ == b.minutes_; }

private:
    std::vector<WeeklyWindow> windows_;
    std::bitset<kMinutesPerWeek> minutes_;
};

}  // namespace sirenproof::core
