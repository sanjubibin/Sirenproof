#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "sirenproof/core/app.hpp"
#include "sirenproof/core/clock.hpp"
#include "sirenproof/core/crypto.hpp"
#include "sirenproof/core/rule.hpp"
#include "sirenproof/core/time.hpp"
#include "sirenproof/core/url.hpp"

namespace sirenproof::core {

// A short stretch during which a rule doesn't apply: "continue for 5 minutes" after a nudge, or a break
// (empty rule_id), which pauses every rule that allows breaks.
struct Allowance {
    std::string rule_id;
    TimePoint from;
    TimePoint until;

    friend bool operator==(const Allowance&, const Allowance&) = default;
};

// A lock ended early by its unlock method; the rule is off until its current period would have ended.
struct Release {
    std::string rule_id;
    TimePoint from;
    TimePoint until;

    friend bool operator==(const Release&, const Release&) = default;
};

struct UnlockRequest {  // Wait method: releases at `due` unless cancelled
    std::string rule_id;
    TimePoint due;

    friend bool operator==(const UnlockRequest&, const UnlockRequest&) = default;
};

struct TypeChallenge {  // Type method: the text that has to be typed out
    std::string rule_id;
    std::string text;

    friend bool operator==(const TypeChallenge&, const TypeChallenge&) = default;
};

struct FriendAttempts {  // Friend method: wrong guesses slow down further guesses
    std::string rule_id;
    int failures = 0;
    TimePoint retry_after{};

    friend bool operator==(const FriendAttempts&, const FriendAttempts&) = default;
};

struct BreakCount {
    std::string date;  // local date the count is for
    int used = 0;

    friend bool operator==(const BreakCount&, const BreakCount&) = default;
};

struct ReplaceRule {
    Rule rule;
    friend bool operator==(const ReplaceRule&, const ReplaceRule&) = default;
};
struct RemoveRule {
    std::string rule_id;
    friend bool operator==(const RemoveRule&, const RemoveRule&) = default;
};
struct ChangeSettings {
    Settings settings;
    friend bool operator==(const ChangeSettings&, const ChangeSettings&) = default;
};

// A loosening change waiting out its delay. It can be cancelled until it applies.
struct PendingChange {
    std::string id;
    std::variant<ReplaceRule, RemoveRule, ChangeSettings> change;
    TimePoint apply_at;

    friend bool operator==(const PendingChange&, const PendingChange&) = default;
};

// Everything Sirenproof remembers. The service owns it and saves it; nothing else edits it directly.
struct State {
    Settings settings;
    std::vector<Rule> rules;
    std::vector<PendingChange> pending;
    std::vector<Allowance> allowances;
    std::vector<Release> releases;
    std::vector<UnlockRequest> unlock_requests;
    std::vector<TypeChallenge> type_challenges;
    std::vector<FriendAttempts> friend_attempts;
    BreakCount breaks;
    std::optional<ClockCheckpoint> clock;
    // The time zone offset weekly schedules run on. Kept, like trusted time, so that changing the time
    // zone and rebooting doesn't shift a schedule while it's in force.
    std::optional<Minutes> local_offset;
    std::uint64_t next_id = 1;

    friend bool operator==(const State&, const State&) = default;
};

enum class Action { Allow, Nudge, Block };
std::string_view to_string(Action action);

struct Verdict {
    Action action = Action::Allow;
    std::string rule_id;  // the deciding rule; empty when allowed
    Strength strength = Strength::Nudge;
    std::optional<TimePoint> until;  // when the deciding rule's current period ends, if it does
    std::string goal;
};

struct PlanEntry {
    std::string target;
    Action action;
    std::string rule_id;
};

// What has to be enforced right now, for the platform layers to apply.
struct Plan {
    std::vector<std::string> active_rules;  // enforcing now (not paused by a break or allowance)
    std::vector<Domain> hosts;              // whole sites safe to put in the hosts file
    std::vector<PlanEntry> web;             // page-level rules, for the browser extension
    std::vector<PlanEntry> apps;            // program rules, for the process watcher
    std::optional<TimePoint> next_change;   // re-plan no later than this
};

struct ChangeResult {
    enum class Outcome { Applied, Pending, Refused };

    Outcome outcome = Outcome::Refused;
    std::string message;
    std::string rule_id;
    std::string pending_id;
    std::optional<TimePoint> applies_at;
    std::string friend_code;  // a new friend-unlock code, shown exactly once
};

struct ActionResult {
    bool ok = false;
    std::string message;
    std::optional<TimePoint> until;
};

// The rules of Sirenproof, applied to a State.
//
// Two promises hold however the engine is driven:
// - Tightening is instant: any change that only restricts more applies at once.
// - Loosening is slow: any other change waits out the rule's loosen delay, and a lock refuses it
//   outright while it holds.
class Engine {
public:
    Engine(State& state, LocalOffsetFn local_offset);

    // Reading
    const Rule* find_rule(std::string_view id) const;
    bool is_enforcing(const Rule& rule, TimePoint now) const;  // in its time, and not unlocked early
    bool lock_holds(const Rule& rule, TimePoint now) const;
    std::optional<TimePoint> period_end(const Rule& rule, TimePoint now) const;
    Verdict decide(const Url& url, TimePoint now) const;
    Verdict decide(const AppIdentity& app, TimePoint now) const;
    Plan plan(TimePoint now) const;
    std::optional<TimePoint> next_change(TimePoint now) const;
    // For TrustedClock: true if jumping from `from` to `to` loosens nothing.
    bool time_change_is_safe(TimePoint from, TimePoint to) const;
    // True if switching to a different local-time offset (a time zone change) loosens nothing now.
    bool offset_change_is_safe(TimePoint now, const LocalOffsetFn& proposed) const;
    // Rules in force at `t`: in their time, not unlocked early and not paused. Sorted by id.
    std::vector<std::string> enforcing_rules(TimePoint t) const;

    // Housekeeping: applies due changes and unlocks, drops anything expired. Call at least at next_change.
    void tick(TimePoint now);

    // Changing rules and settings
    ChangeResult add_rule(Rule rule, TimePoint now, RandomSource& random);
    ChangeResult update_rule(Rule rule, TimePoint now, RandomSource& random);
    ChangeResult remove_rule(std::string_view rule_id, TimePoint now);
    ChangeResult update_settings(const Settings& settings, TimePoint now);
    bool cancel_pending(std::string_view pending_id);

    // In the moment
    ActionResult continue_after_nudge(std::string_view rule_id, TimePoint now);
    ActionResult start_break(TimePoint now);
    ActionResult request_unlock(std::string_view rule_id, TimePoint now);
    bool cancel_unlock(std::string_view rule_id);
    std::optional<std::string> begin_type_unlock(std::string_view rule_id, TimePoint now, RandomSource& random);
    ActionResult finish_type_unlock(std::string_view rule_id, std::string_view typed, TimePoint now);
    ActionResult enter_friend_code(std::string_view rule_id, std::string_view code, TimePoint now);

    static constexpr Minutes kSettingsLoosenDelay{10};
    static constexpr std::size_t kFriendCodeLength = 15;
    static constexpr int kFriendAttemptsBeforeWait = 5;

private:
    Rule* find_rule_mutable(std::string_view id);
    bool is_paused(const Rule& rule, TimePoint now) const;
    bool any_lock_holds(TimePoint now) const;
    void release(const Rule& rule, TimePoint now);
    void forget_rule(std::string_view rule_id);
    void drop_pending_for(std::string_view rule_id);
    std::string new_id(std::string_view prefix);
    void ensure_friend_code(Rule& rule, const Rule* before, RandomSource& random, ChangeResult& result);
    template <class Matches>
    Verdict decide_with(Matches matches, TimePoint now) const;

    State& state_;
    LocalOffsetFn local_offset_;
};

}  // namespace sirenproof::core
