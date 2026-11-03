#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

#include "sirenproof/core/rule.hpp"

namespace sirenproof::core {

using Json = nlohmann::ordered_json;

// Malformed input. The message starts with the path of the offending field, e.g.
// "rule.block[1]: not a valid target".
class JsonError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// The same strict conversions the state file uses, for messages that carry a rule or settings.
// Readers throw JsonError; rule_from_json also re-checks the safety rails.
Rule rule_from_json(const Json& value, const std::string& path);
Json rule_to_json(const Rule& rule);
When when_from_json(const Json& value, const std::string& path);
Json when_to_json(const When& when);
Settings settings_from_json(const Json& value, const std::string& path);
Json settings_to_json(const Settings& settings);

// True if JSON `text` nests arrays and objects deeper than `limit`. Copying, comparing and printing a
// parsed value recurse once per level, so a small message of 30,000 "[" could exhaust the stack; check
// untrusted messages with this before parsing them. Brackets inside strings don't count.
bool nested_deeper_than(std::string_view text, int limit);
inline constexpr int kMaxMessageDepth = 32;  // real messages nest five or six levels

}  // namespace sirenproof::core
