#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sirenproof/core/domain.hpp"

namespace sirenproof::core {

// The parts of a web address that rules look at.
struct Url {
    Domain host;
    std::string path;   // normalised (see normalise_path)
    std::string query;  // raw, without the '?'

    // Accepts "https://www.youtube.com/watch?v=x", "http://..." or a bare "youtube.com/feed".
    // Ports and user info are dropped; the fragment is ignored. Other schemes (chrome://, file:,
    // about:) and IP-address hosts give nullopt.
    static std::optional<Url> parse(std::string_view text);
};

// Path form used for matching, so trivial variations can't slip past a rule: starts with '/',
// lower-case ASCII, repeated slashes collapsed, and %XX escapes of unreserved characters decoded
// ("/%53horts" is "/shorts").
std::string normalise_path(std::string_view path);

struct QueryParam {
    std::string key;
    std::optional<std::string> value;  // nullopt: the key just has to be present

    friend bool operator==(const QueryParam&, const QueryParam&) = default;
};

// A rule target for part of a site: "youtube.com/shorts", "youtube.com/" or "youtube.com/watch?v=abc".
//
// - The host covers its subdomains, like a Domain does.
// - A path matches itself and everything below it, on whole segments: "/shorts" matches "/shorts" and
//   "/shorts/abc" but not "/shortsfoo".
// - The root path "/" matches only the home page; to target a whole site, use a Domain.
// - Every query parameter in the pattern must be present in the address (others may be too). Values
//   are case-sensitive, because video IDs are.
struct PagePattern {
    Domain host;
    std::string path;
    std::vector<QueryParam> query;

    static std::optional<PagePattern> parse(std::string_view text);
    bool matches(const Url& url) const;
    std::string to_string() const;

    friend bool operator==(const PagePattern&, const PagePattern&) = default;
};

}  // namespace sirenproof::core
