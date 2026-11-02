#pragma once

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sirenproof/core/rule.hpp"
#include "sirenproof/core/time.hpp"

namespace sirenproof::core {

// Ready-made lists of common distractions, so a lock or a rule can be set up with one choice.
struct ReadyList {
    std::string_view id;
    std::string_view label;
    std::string_view detail;  // what's in it, in a few words
    std::string_view effect;  // what blocking it does, in a sentence
    std::span<const std::string_view> targets;
};

std::span<const ReadyList> ready_lists();
const ReadyList* find_ready_list(std::string_view id);

// The targets a well-known name stands for ("youtube" is youtube.com, "steam" is app:steam.exe), or an
// empty span if it isn't one.
std::span<const std::string_view> named_targets(std::string_view name);

// A command typed into the main window's palette. It's the command line's grammar with shortcuts for
// typing fast: a bare duration means --for ("block reddit.com 25m"), "nudge" and "lock" are verbs of
// their own, and a ready-made list or a well-known name stands for its targets ("lock video 2h",
// "block reddit 25m").
//
//   block|nudge <target>... [<duration>]       lock <target>... <duration> [--unlock <method>]
//   break    check <address-or-program>    now | rules | lock | unlock | lockdown
struct Command {
    enum class Kind { Block, Lock, Break, Check, Go };

    Kind kind = Kind::Go;
    std::vector<std::string> lists;    // ready-made lists named, by id (Block, Lock)
    std::vector<std::string> targets;  // every other target, names expanded (Block, Lock)
    std::optional<Minutes> length;     // Block: optional, for a session; Lock: required
    Strength strength = Strength::Block;
    std::string unlock;   // Lock: none, wait, type or friend; empty to choose on screen
    std::string subject;  // Check
    std::string screen;   // Go: now, rules, lock or lockdown
};

struct CommandError {
    std::string message;
    bool known_verb = false;  // false when the first word isn't a command at all
};

std::expected<Command, CommandError> parse_command(std::string_view text);

}  // namespace sirenproof::core
