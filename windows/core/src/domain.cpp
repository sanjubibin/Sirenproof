#include "sirenproof/core/domain.hpp"

#include <algorithm>
#include <cstddef>

namespace sirenproof::core {

namespace {

constexpr std::size_t kMaxNameLength = 253;
constexpr std::size_t kMaxLabelLength = 63;

constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }

// Letter, digit or hyphen, after lower-casing.
constexpr bool is_ldh(char c) { return (c >= 'a' && c <= 'z') || is_digit(c) || c == '-'; }

constexpr char to_lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool is_valid_label(std::string_view label) {
    if (label.empty() || label.size() > kMaxLabelLength) return false;
    if (label.front() == '-' || label.back() == '-') return false;
    return std::ranges::all_of(label, is_ldh);
}

}  // namespace

std::optional<Domain> Domain::parse(std::string_view text) {
    if (text.ends_with('.')) text.remove_suffix(1);
    if (text.empty() || text.size() > kMaxNameLength) return std::nullopt;

    std::string name(text.size(), '\0');
    std::ranges::transform(text, name.begin(), to_lower_ascii);

    std::size_t label_count = 0;
    std::string_view last_label;
    for (std::size_t start = 0;;) {
        const std::size_t dot = name.find('.', start);
        const std::string_view label =
            std::string_view(name).substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!is_valid_label(label)) return std::nullopt;
        ++label_count;
        last_label = label;
        if (dot == std::string::npos) break;
        start = dot + 1;
    }

    if (label_count < 2 || std::ranges::all_of(last_label, is_digit)) return std::nullopt;
    return Domain(std::move(name));
}

bool Domain::covers(const Domain& host) const noexcept {
    const std::string_view h = host.name_;
    if (!h.ends_with(name_)) return false;
    return h.size() == name_.size() || h[h.size() - name_.size() - 1] == '.';
}

}  // namespace sirenproof::core
