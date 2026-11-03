#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "sirenproof/core/schedule.hpp"
#include "sirenproof/core/target.hpp"
#include "sirenproof/core/time.hpp"

namespace sirenproof::core {

// How hard a rule holds. Ordered: a higher value is stricter.
enum class Strength {
    Nudge,  // a pause showing your goal, then you may continue for a few minutes
    Block,  // blocked; loosening the rule waits out its delay
    Lock,   // blocked; can't be loosened until it ends, except by the unlock method chosen up front
};

std::string_view to_string(Strength strength);
std::optional<Strength> parse_strength(std::string_view text);

// When a rule applies.
struct Always {
    friend bool operator==(Always, Always) = default;
};
struct Span {  // a one-off session, in trusted time
    TimePoint start;
    TimePoint end;

    friend bool operator==(const Span&, const Span&) = default;
};
using When = std::variant<Always, WeeklySchedule, Span>;

// How a Lock can end early. Chosen when the lock is created.
struct UnlockMethod {
    enum class Kind {
        None,    // it holds until it ends
        Wait,    // ask, then wait `wait` before it releases
        Type,    // copy out `type_length` random characters by hand
        Friend,  // enter a code that was shown once and handed to someone you trust
    };

    Kind kind = Kind::None;
    Minutes wait{0};
    int type_length = 0;
    std::string friend_salt;  // hex; the code itself is never stored
    std::string friend_hash;  // hex SHA-256 of salt + normalised code

    friend bool operator==(const UnlockMethod&, const UnlockMethod&) = default;
};

struct Rule {
    std::string id;
    std::string name;
    std::string goal;  // shown when something is blocked, in your own words
    std::vector<Target> block;
    std::vector<Target> allow;  // exceptions to this rule's own blocks, never to other rules
    When when = Always{};
    Strength strength = Strength::Block;
    Minutes loosen_delay{10};
    UnlockMethod unlock;          // Lock only
    bool breaks_allowed = false;  // Lock only; Nudge and Block rules always pause for breaks

    friend bool operator==(const Rule&, const Rule&) = default;
};

struct Settings {
    Hours max_lock{24};      // the longest a Lock may run without a break in its schedule
    int breaks_per_day = 3;
    Minutes break_length{5};
    Seconds nudge_pause{15};
    Minutes nudge_continue{5};

    friend bool operator==(const Settings&, const Settings&) = default;
};

// Why the rule can't be saved, or nullopt if it's valid under `settings`.
std::optional<std::string> validate_rule(const Rule& rule, const Settings& settings);
std::optional<std::string> validate_settings(const Settings& settings);

// True if `after` restricts at least as much as `before`: it blocks everything `before` blocked, allows
// nothing new, applies at every time `before` did, at least as strongly, with at least the same delay
// and an unlock method that is no easier. Anything else is a loosening and has to wait.
bool is_tightening(const Rule& before, const Rule& after);
bool is_tightening(const Settings& before, const Settings& after);

}  // namespace sirenproof::core
