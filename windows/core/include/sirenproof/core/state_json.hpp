#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "sirenproof/core/policy.hpp"

namespace sirenproof::core {

inline constexpr int kStateFormatVersion = 1;

// Reads a state file. Strict on purpose: unknown fields, wrong types, invalid targets and rules that
// break the safety rails are all errors, so a typo can't silently weaken a block. The error names the
// offending field, e.g. "rules[2].block[0]: not a valid target".
std::expected<State, std::string> parse_state(std::string_view json_text);

// Writes a state file, pretty-printed. parse_state(serialise_state(s)) == s.
std::string serialise_state(const State& state);

}  // namespace sirenproof::core
