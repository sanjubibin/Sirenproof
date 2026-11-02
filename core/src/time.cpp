#include "sirenproof/core/time.hpp"

#include <format>

namespace sirenproof::core {

namespace {

// Parses exactly `width` decimal digits.
std::optional<int> parse_digits(std::string_view text, std::size_t pos, std::size_t width) {
    if (pos + width > text.size()) return std::nullopt;
    int value = 0;
    for (std::size_t i = pos; i < pos + width; ++i) {
        const char c = text[i];
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + (c - '0');
    }
    return value;
}

}  // namespace

int minute_of_week(TimePoint t, Minutes local_offset) {
    using namespace std::chrono;
    const auto local = floor<minutes>(t) + local_offset;
    const auto day = floor<days>(local);
    const unsigned iso_day = weekday{day}.iso_encoding();  // Monday = 1 ... Sunday = 7
    const auto minute_of_day = (local - day).count();
    return static_cast<int>(iso_day - 1) * kMinutesPerDay + static_cast<int>(minute_of_day);
}

std::string local_date(TimePoint t, Minutes local_offset) {
    using namespace std::chrono;
    const year_month_day ymd{floor<days>(t + local_offset)};
    return std::format("{:%F}", ymd);
}

std::string format_time(TimePoint t) { return std::format("{:%FT%TZ}", t); }

std::optional<TimePoint> parse_time(std::string_view text) {
    using namespace std::chrono;
    // YYYY-MM-DDTHH:MM:SSZ
    if (text.size() != 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' || text[13] != ':' ||
        text[16] != ':' || text[19] != 'Z') {
        return std::nullopt;
    }
    const auto y = parse_digits(text, 0, 4);
    const auto mo = parse_digits(text, 5, 2);
    const auto d = parse_digits(text, 8, 2);
    const auto h = parse_digits(text, 11, 2);
    const auto mi = parse_digits(text, 14, 2);
    const auto s = parse_digits(text, 17, 2);
    if (!y || !mo || !d || !h || !mi || !s) return std::nullopt;
    if (*h > 23 || *mi > 59 || *s > 59) return std::nullopt;

    const year_month_day ymd{year{*y}, month{static_cast<unsigned>(*mo)}, day{static_cast<unsigned>(*d)}};
    if (!ymd.ok()) return std::nullopt;
    return sys_days{ymd} + hours{*h} + minutes{*mi} + seconds{*s};
}

std::string format_clock(int minutes) { return std::format("{:02}:{:02}", minutes / 60, minutes % 60); }

std::optional<int> parse_clock(std::string_view text) {
    if (text.size() != 5 || text[2] != ':') return std::nullopt;
    const auto h = parse_digits(text, 0, 2);
    const auto m = parse_digits(text, 3, 2);
    if (!h || !m || *m > 59) return std::nullopt;
    const int total = *h * 60 + *m;
    if (total > kMinutesPerDay) return std::nullopt;
    return total;
}

std::optional<Minutes> parse_duration(std::string_view text) {
    int total = 0;
    int number = -1;
    for (const char c : text) {
        if (c >= '0' && c <= '9') {
            number = (number < 0 ? 0 : number) * 10 + (c - '0');
            if (number > 100'000) return std::nullopt;
        } else if ((c == 'h' || c == 'm') && number >= 0) {
            total += c == 'h' ? number * 60 : number;
            number = -1;
        } else {
            return std::nullopt;
        }
    }
    if (number >= 0) total += number;
    return total > 0 ? std::optional(Minutes{total}) : std::nullopt;
}

}  // namespace sirenproof::core
