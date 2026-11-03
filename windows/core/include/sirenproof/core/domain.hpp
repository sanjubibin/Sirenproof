#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sirenproof::core {

// A validated, normalised DNS host name such as "reddit.com" or "m.youtube.com".
//
// Every domain Sirenproof stores or writes (rules, the hosts file) goes through Domain::parse, so a
// Domain can never carry whitespace, newlines or other characters that could smuggle extra entries
// into the hosts file.
class Domain {
public:
    // Lower-cases ASCII letters and drops one trailing dot, then checks the result is a host name:
    // - labels of 1-63 letters, digits or hyphens, with no hyphen at either end;
    // - at most 253 characters in total;
    // - at least two labels, so a rule can never target a whole top-level domain like "com";
    // - a last label that isn't all digits, so IPv4 addresses are rejected.
    // Internationalised names must be given in punycode ("xn--..."). Returns nullopt if invalid.
    static std::optional<Domain> parse(std::string_view text);

    const std::string& str() const noexcept { return name_; }

    // True if `host` is this domain or one of its subdomains: "reddit.com" covers "old.reddit.com"
    // but not "notreddit.com".
    bool covers(const Domain& host) const noexcept;

    friend bool operator==(const Domain&, const Domain&) = default;
    friend std::strong_ordering operator<=>(const Domain&, const Domain&) = default;

private:
    explicit Domain(std::string name) : name_(std::move(name)) {}

    std::string name_;
};

}  // namespace sirenproof::core
