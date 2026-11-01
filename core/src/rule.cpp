#include "sirenproof/core/rule.hpp"

#include <algorithm>

#include "sirenproof/core/crypto.hpp"

namespace sirenproof::core {

namespace {

constexpr std::size_t kMaxTargets = 1000;
constexpr std::size_t kMaxIdLength = 64;
constexpr std::size_t kMaxNameLength = 100;
constexpr std::size_t kMaxGoalLength = 300;
constexpr Minutes kMaxLoosenDelay = std::chrono::days{7};

bool has_control_char(std::string_view text) {
    return std::ranges::any_of(text, [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u < 0x20 || u == 0x7f;
    });
}

bool is_valid_id(std::string_view id) {
    return !id.empty() && id.size() <= kMaxIdLength && std::ranges::all_of(id, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}

// True if `outer` applies at every moment `inner` does.
bool covers(const When& outer, const When& inner) {
    if (std::holds_alternative<Always>(outer)) return true;
    if (const auto* o = std::get_if<WeeklySchedule>(&outer)) {
        if (const auto* i = std::get_if<WeeklySchedule>(&inner)) return o->contains(*i);
        return false;  // a weekly schedule can't be compared with Always or a one-off span
    }
    const auto& o = std::get<Span>(outer);
    if (const auto* i = std::get_if<Span>(&inner)) return o.start <= i->start && o.end >= i->end;
    return false;
}

bool unlock_no_easier(const UnlockMethod& before, const UnlockMethod& after) {
    using Kind = UnlockMethod::Kind;
    if (after.kind == Kind::None) return true;
    if (after.kind != before.kind) return false;
    switch (after.kind) {
        case Kind::Wait: return after.wait >= before.wait;
        case Kind::Type: return after.type_length >= before.type_length;
        case Kind::Friend: return after.friend_hash == before.friend_hash;
        case Kind::None: return true;
    }
    return false;
}

template <class T>
bool contains_all(const std::vector<T>& haystack, const std::vector<T>& needles) {
    return std::ranges::all_of(needles, [&](const T& n) { return std::ranges::find(haystack, n) != haystack.end(); });
}

}  // namespace

std::string_view to_string(Strength strength) {
    switch (strength) {
        case Strength::Nudge: return "nudge";
        case Strength::Block: return "block";
        case Strength::Lock: return "lock";
    }
    return "block";
}

std::optional<Strength> parse_strength(std::string_view text) {
    if (text == "nudge") return Strength::Nudge;
    if (text == "block") return Strength::Block;
    if (text == "lock") return Strength::Lock;
    return std::nullopt;
}

std::optional<std::string> validate_rule(const Rule& rule, const Settings& settings) {
    if (!is_valid_id(rule.id)) return "the id must be 1-64 lower-case letters, digits, '-' or '_'";
    if (rule.name.size() > kMaxNameLength || has_control_char(rule.name)) return "the name is too long or invalid";
    if (rule.goal.size() > kMaxGoalLength || has_control_char(rule.goal)) return "the goal is too long or invalid";
    if (rule.block.empty()) return "a rule needs at least one thing to block";
    if (rule.block.size() > kMaxTargets || rule.allow.size() > kMaxTargets) return "too many targets in one rule";
    for (const Target& target : rule.block) {
        if (auto reason = protected_reason(target)) return "can't block " + to_string(target) + ": " + *reason;
    }
    if (rule.loosen_delay < Minutes{0} || rule.loosen_delay > kMaxLoosenDelay) {
        return "the loosen delay must be between 0 minutes and 7 days";
    }

    if (const auto* span = std::get_if<Span>(&rule.when); span && span->end <= span->start) {
        return "a session must end after it starts";
    }
    if (const auto* weekly = std::get_if<WeeklySchedule>(&rule.when); weekly && weekly->windows().empty()) {
        return "a schedule needs at least one window";
    }

    using Kind = UnlockMethod::Kind;
    if (rule.strength != Strength::Lock) {
        if (rule.unlock.kind != Kind::None) return "only a lock has an unlock method";
        return std::nullopt;
    }

    // Lock-only checks: it must end, and not run longer than the cap.
    const Minutes cap = settings.max_lock;
    if (std::holds_alternative<Always>(rule.when)) return "a lock needs an end: give it a session or a schedule";
    if (const auto* span = std::get_if<Span>(&rule.when); span && span->end - span->start > cap) {
        return "a lock can run for at most " + std::to_string(settings.max_lock.count()) + " hours";
    }
    if (const auto* weekly = std::get_if<WeeklySchedule>(&rule.when);
        weekly && Minutes{weekly->longest_active_run()} > cap) {
        return "a scheduled lock can't stay on for more than " + std::to_string(settings.max_lock.count()) +
               " hours in a row";
    }

    switch (rule.unlock.kind) {
        case Kind::None: break;
        case Kind::Wait:
            if (rule.unlock.wait < Minutes{1} || rule.unlock.wait > Hours{24}) {
                return "the unlock wait must be between 1 minute and 24 hours";
            }
            break;
        case Kind::Type:
            if (rule.unlock.type_length < 20 || rule.unlock.type_length > 2000) {
                return "the text to type must be 20-2000 characters long";
            }
            break;
        case Kind::Friend: {
            const bool unset = rule.unlock.friend_salt.empty() && rule.unlock.friend_hash.empty();
            const auto salt = from_hex(rule.unlock.friend_salt);
            const auto hash = from_hex(rule.unlock.friend_hash);
            if (!unset && (!salt || salt->size() < 16 || !hash || hash->size() != 32)) {
                return "the friend code record is damaged";
            }
            break;
        }
    }
    return std::nullopt;
}

std::optional<std::string> validate_settings(const Settings& s) {
    if (s.max_lock < Hours{1} || s.max_lock > Hours{24 * 7}) return "the lock cap must be 1-168 hours";
    if (s.breaks_per_day < 0 || s.breaks_per_day > 20) return "breaks per day must be 0-20";
    if (s.break_length < Minutes{1} || s.break_length > Minutes{60}) return "a break must be 1-60 minutes";
    if (s.nudge_pause < Seconds{3} || s.nudge_pause > Seconds{300}) return "the nudge pause must be 3-300 seconds";
    if (s.nudge_continue < Minutes{1} || s.nudge_continue > Minutes{60}) {
        return "continuing after a nudge must last 1-60 minutes";
    }
    return std::nullopt;
}

bool is_tightening(const Rule& before, const Rule& after) {
    return contains_all(after.block, before.block) &&    // still blocks everything
           contains_all(before.allow, after.allow) &&    // no new exceptions
           covers(after.when, before.when) &&            // applies at least as often
           after.strength >= before.strength &&          // at least as strong
           after.loosen_delay >= before.loosen_delay &&  // at least as slow to loosen
           unlock_no_easier(before.unlock, after.unlock) && (!after.breaks_allowed || before.breaks_allowed);
}

bool is_tightening(const Settings& before, const Settings& after) {
    return after.max_lock >= before.max_lock && after.breaks_per_day <= before.breaks_per_day &&
           after.break_length <= before.break_length && after.nudge_pause >= before.nudge_pause &&
           after.nudge_continue <= before.nudge_continue;
}

}  // namespace sirenproof::core
