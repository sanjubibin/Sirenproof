#include "sirenproof/core/policy.hpp"

#include <algorithm>
#include <array>
#include <iterator>

namespace sirenproof::core {

namespace {

Action action_for(Strength strength) { return strength == Strength::Nudge ? Action::Nudge : Action::Block; }

bool matches_url(const Target& target, const Url& url) {
    if (const auto* domain = std::get_if<Domain>(&target)) return domain->covers(url.host);
    if (const auto* page = std::get_if<PagePattern>(&target)) return page->matches(url);
    return false;
}

bool matches_app(const Target& target, const AppIdentity& app) {
    if (const auto* pattern = std::get_if<AppPattern>(&target)) return pattern->matches(app);
    return false;
}

// A whole-site block can only go in the hosts file if the rule allows nothing on that site: the hosts
// file can't make exceptions for single pages or subdomains.
bool has_exception_on(const Rule& rule, const Domain& site) {
    return std::ranges::any_of(rule.allow, [&](const Target& target) {
        if (const auto* domain = std::get_if<Domain>(&target)) return site.covers(*domain) || domain->covers(site);
        if (const auto* page = std::get_if<PagePattern>(&target)) {
            return site.covers(page->host) || page->host.covers(site);
        }
        return false;
    });
}

// True if a verdict lasting until `a` outlasts one lasting until `b` (nullopt means no end).
bool lasts_longer(const std::optional<TimePoint>& a, const std::optional<TimePoint>& b) {
    if (!a) return b.has_value();
    return b && *a > *b;
}

bool refers_to(const PendingChange& pending, std::string_view rule_id) {
    if (const auto* replace = std::get_if<ReplaceRule>(&pending.change)) return replace->rule.id == rule_id;
    if (const auto* remove = std::get_if<RemoveRule>(&pending.change)) return remove->rule_id == rule_id;
    return false;
}

ChangeResult refused(std::string message) {
    ChangeResult result;
    result.outcome = ChangeResult::Outcome::Refused;
    result.message = std::move(message);
    return result;
}

ActionResult failed(std::string message, std::optional<TimePoint> until = std::nullopt) {
    return {false, std::move(message), until};
}

}  // namespace

std::string_view to_string(Action action) {
    switch (action) {
        case Action::Allow: return "allow";
        case Action::Nudge: return "nudge";
        case Action::Block: return "block";
    }
    return "allow";
}

Engine::Engine(State& state, LocalOffsetFn local_offset) : state_(state), local_offset_(std::move(local_offset)) {}

// ---------------------------------------------------------------------------------------------------
// Reading

const Rule* Engine::find_rule(std::string_view id) const {
    const auto it = std::ranges::find(state_.rules, id, &Rule::id);
    return it == state_.rules.end() ? nullptr : &*it;
}

Rule* Engine::find_rule_mutable(std::string_view id) {
    const auto it = std::ranges::find(state_.rules, id, &Rule::id);
    return it == state_.rules.end() ? nullptr : &*it;
}

bool Engine::is_enforcing(const Rule& rule, TimePoint now) const {
    struct InTime {
        TimePoint at;
        Minutes local_offset;
        bool operator()(const Always&) const { return true; }
        bool operator()(const WeeklySchedule& s) const { return s.active_at(minute_of_week(at, local_offset)); }
        bool operator()(const Span& s) const { return s.start <= at && at < s.end; }
    };
    if (!std::visit(InTime{now, local_offset_(now)}, rule.when)) return false;
    return std::ranges::none_of(state_.releases, [&](const Release& r) {
        return r.rule_id == rule.id && r.from <= now && now < r.until;
    });
}

bool Engine::lock_holds(const Rule& rule, TimePoint now) const {
    return rule.strength == Strength::Lock && is_enforcing(rule, now);
}

bool Engine::any_lock_holds(TimePoint now) const {
    return std::ranges::any_of(state_.rules, [&](const Rule& rule) { return lock_holds(rule, now); });
}

std::optional<TimePoint> Engine::period_end(const Rule& rule, TimePoint now) const {
    if (const auto* span = std::get_if<Span>(&rule.when)) return span->end;
    if (const auto* weekly = std::get_if<WeeklySchedule>(&rule.when)) {
        const int minutes = weekly->minutes_until_change(minute_of_week(now, local_offset_(now)));
        if (minutes >= kMinutesPerWeek) return std::nullopt;
        return std::chrono::floor<Minutes>(now) + Minutes{minutes};
    }
    return std::nullopt;
}

bool Engine::is_paused(const Rule& rule, TimePoint now) const {
    const bool takes_breaks = rule.strength != Strength::Lock || rule.breaks_allowed;
    return std::ranges::any_of(state_.allowances, [&](const Allowance& a) {
        if (a.from > now || now >= a.until) return false;
        return a.rule_id == rule.id || (a.rule_id.empty() && takes_breaks);
    });
}

template <class Matches>
Verdict Engine::decide_with(Matches matches, TimePoint now) const {
    Verdict best;
    for (const Rule& rule : state_.rules) {
        if (!is_enforcing(rule, now) || is_paused(rule, now)) continue;
        if (std::ranges::any_of(rule.allow, matches)) continue;
        if (std::ranges::none_of(rule.block, matches)) continue;

        Verdict candidate{action_for(rule.strength), rule.id, rule.strength, period_end(rule, now), rule.goal};
        const bool stronger = best.action == Action::Allow || rule.strength > best.strength;
        const bool longer = rule.strength == best.strength && lasts_longer(candidate.until, best.until);
        if (stronger || longer) best = std::move(candidate);
    }
    return best;
}

Verdict Engine::decide(const Url& url, TimePoint now) const {
    return decide_with([&](const Target& t) { return matches_url(t, url); }, now);
}

Verdict Engine::decide(const AppIdentity& app, TimePoint now) const {
    return decide_with([&](const Target& t) { return matches_app(t, app); }, now);
}

Plan Engine::plan(TimePoint now) const {
    Plan plan;
    for (const Rule& rule : state_.rules) {
        if (!is_enforcing(rule, now) || is_paused(rule, now)) continue;
        plan.active_rules.push_back(rule.id);

        const Action action = action_for(rule.strength);
        for (const Target& target : rule.block) {
            if (!is_web_target(target)) {
                plan.apps.push_back({to_string(target), action, rule.id});
                continue;
            }
            plan.web.push_back({to_string(target), action, rule.id});
            const auto* domain = std::get_if<Domain>(&target);
            if (domain && action == Action::Block && !has_exception_on(rule, *domain)) plan.hosts.push_back(*domain);
        }
        for (const Target& target : rule.allow) {
            (is_web_target(target) ? plan.web : plan.apps).push_back({to_string(target), Action::Allow, rule.id});
        }
    }
    std::ranges::sort(plan.hosts);
    const auto duplicates = std::ranges::unique(plan.hosts);
    plan.hosts.erase(duplicates.begin(), duplicates.end());
    plan.next_change = next_change(now);
    return plan;
}

std::optional<TimePoint> Engine::next_change(TimePoint now) const {
    // Anything already due needs a tick straight away.
    const bool overdue = std::ranges::any_of(state_.pending, [&](const auto& p) { return p.apply_at <= now; }) ||
                         std::ranges::any_of(state_.unlock_requests, [&](const auto& u) { return u.due <= now; });
    if (overdue) return now;

    std::optional<TimePoint> earliest;
    const auto consider = [&](TimePoint t) {
        if (t > now && (!earliest || t < *earliest)) earliest = t;
    };
    for (const Rule& rule : state_.rules) {
        if (const auto* span = std::get_if<Span>(&rule.when)) {
            consider(span->start);
            consider(span->end);
        } else if (const auto* weekly = std::get_if<WeeklySchedule>(&rule.when)) {
            const int minutes = weekly->minutes_until_change(minute_of_week(now, local_offset_(now)));
            if (minutes < kMinutesPerWeek) consider(std::chrono::floor<Minutes>(now) + Minutes{minutes});
        }
    }
    for (const Allowance& a : state_.allowances) {
        consider(a.from);
        consider(a.until);
    }
    for (const Release& r : state_.releases) consider(r.until);
    for (const PendingChange& p : state_.pending) consider(p.apply_at);
    for (const UnlockRequest& u : state_.unlock_requests) consider(u.due);
    return earliest;
}

std::vector<std::string> Engine::enforcing_rules(TimePoint t) const {
    std::vector<std::string> ids;
    for (const Rule& rule : state_.rules) {
        if (is_enforcing(rule, t) && !is_paused(rule, t)) ids.push_back(rule.id);
    }
    std::ranges::sort(ids);
    return ids;
}

bool Engine::time_change_is_safe(TimePoint from, TimePoint to) const {
    // Everything in force at `from` must still be in force at `to`...
    if (!std::ranges::includes(enforcing_rules(to), enforcing_rules(from))) return false;
    // ...and jumping forward mustn't bring a waiting loosening or unlock due early.
    if (to > from) {
        const auto skipped = [&](TimePoint t) { return t > from && t <= to; };
        if (std::ranges::any_of(state_.pending, [&](const auto& p) { return skipped(p.apply_at); })) return false;
        if (std::ranges::any_of(state_.unlock_requests, [&](const auto& u) { return skipped(u.due); })) return false;
    }
    return true;
}

bool Engine::offset_change_is_safe(TimePoint now, const LocalOffsetFn& proposed) const {
    const Engine with_proposed(state_, proposed);
    return std::ranges::includes(with_proposed.enforcing_rules(now), enforcing_rules(now));
}

// ---------------------------------------------------------------------------------------------------
// Housekeeping

void Engine::tick(TimePoint now) {
    // Wait-unlocks that are due release their lock.
    std::vector<std::string> unlocking;
    for (const UnlockRequest& u : state_.unlock_requests) {
        if (u.due <= now) unlocking.push_back(u.rule_id);
    }
    std::erase_if(state_.unlock_requests, [&](const UnlockRequest& u) { return u.due <= now; });
    for (const std::string& id : unlocking) {
        if (const Rule* rule = find_rule(id); rule && lock_holds(*rule, now)) release(*rule, now);
    }

    // Loosenings that are due apply, unless a lock has started holding in the meantime.
    std::vector<std::string> removed;
    std::vector<PendingChange> still_waiting;
    for (PendingChange& pending : state_.pending) {
        if (pending.apply_at > now) {
            still_waiting.push_back(std::move(pending));
            continue;
        }
        bool done = true;
        if (auto* replace = std::get_if<ReplaceRule>(&pending.change)) {
            if (Rule* rule = find_rule_mutable(replace->rule.id)) {
                if (lock_holds(*rule, now)) {
                    done = false;
                } else {
                    *rule = std::move(replace->rule);
                }
            }
        } else if (const auto* remove = std::get_if<RemoveRule>(&pending.change)) {
            if (const Rule* rule = find_rule(remove->rule_id)) {
                if (lock_holds(*rule, now)) {
                    done = false;
                } else {
                    removed.push_back(remove->rule_id);
                    std::erase_if(state_.rules, [&](const Rule& r) { return r.id == remove->rule_id; });
                }
            }
        } else if (auto* settings = std::get_if<ChangeSettings>(&pending.change)) {
            if (any_lock_holds(now)) {
                done = false;
            } else {
                state_.settings = settings->settings;
            }
        }
        if (!done) still_waiting.push_back(std::move(pending));
    }
    state_.pending = std::move(still_waiting);

    // Finished sessions go; so does everything that has expired or lost its lock.
    for (const Rule& rule : state_.rules) {
        if (const auto* span = std::get_if<Span>(&rule.when); span && span->end <= now) removed.push_back(rule.id);
    }
    std::erase_if(state_.rules, [&](const Rule& r) { return std::ranges::find(removed, r.id) != removed.end(); });
    for (const std::string& id : removed) forget_rule(id);

    std::erase_if(state_.allowances, [&](const Allowance& a) { return a.until <= now; });
    std::erase_if(state_.releases, [&](const Release& r) { return r.until <= now; });
    const auto lock_gone = [&](const std::string& id) {
        const Rule* rule = find_rule(id);
        return rule == nullptr || !lock_holds(*rule, now);
    };
    std::erase_if(state_.unlock_requests, [&](const UnlockRequest& u) { return lock_gone(u.rule_id); });
    std::erase_if(state_.type_challenges, [&](const TypeChallenge& c) { return lock_gone(c.rule_id); });
    std::erase_if(state_.friend_attempts, [&](const FriendAttempts& f) { return find_rule(f.rule_id) == nullptr; });
}

void Engine::release(const Rule& rule, TimePoint now) {
    const std::string id = rule.id;
    state_.releases.push_back({id, now, period_end(rule, now).value_or(now + Hours{24})});
    std::erase_if(state_.unlock_requests, [&](const UnlockRequest& u) { return u.rule_id == id; });
    std::erase_if(state_.type_challenges, [&](const TypeChallenge& c) { return c.rule_id == id; });
    std::erase_if(state_.friend_attempts, [&](const FriendAttempts& f) { return f.rule_id == id; });
}

void Engine::forget_rule(std::string_view rule_id) {
    std::erase_if(state_.allowances, [&](const Allowance& a) { return a.rule_id == rule_id; });
    std::erase_if(state_.releases, [&](const Release& r) { return r.rule_id == rule_id; });
    std::erase_if(state_.unlock_requests, [&](const UnlockRequest& u) { return u.rule_id == rule_id; });
    std::erase_if(state_.type_challenges, [&](const TypeChallenge& c) { return c.rule_id == rule_id; });
    std::erase_if(state_.friend_attempts, [&](const FriendAttempts& f) { return f.rule_id == rule_id; });
    drop_pending_for(rule_id);
}

void Engine::drop_pending_for(std::string_view rule_id) {
    std::erase_if(state_.pending, [&](const PendingChange& p) { return refers_to(p, rule_id); });
}

std::string Engine::new_id(std::string_view prefix) {
    for (;;) {
        std::string id = std::string(prefix) + std::to_string(state_.next_id++);
        const bool taken = find_rule(id) != nullptr ||
                           std::ranges::any_of(state_.pending, [&](const auto& p) { return p.id == id; });
        if (!taken) return id;
    }
}

// ---------------------------------------------------------------------------------------------------
// Changing rules and settings

void Engine::ensure_friend_code(Rule& rule, const Rule* before, RandomSource& random, ChangeResult& result) {
    if (rule.strength != Strength::Lock || rule.unlock.kind != UnlockMethod::Kind::Friend) return;
    if (!rule.unlock.friend_hash.empty()) return;
    if (before && before->unlock.kind == UnlockMethod::Kind::Friend && !before->unlock.friend_hash.empty()) {
        rule.unlock.friend_salt = before->unlock.friend_salt;  // same code as before
        rule.unlock.friend_hash = before->unlock.friend_hash;
        return;
    }
    std::array<std::uint8_t, 16> salt{};
    random.fill(salt);
    const std::string code = random_code(random, kFriendCodeLength, true);
    rule.unlock.friend_salt = to_hex(salt);
    rule.unlock.friend_hash = to_hex(sha256(rule.unlock.friend_salt + *normalise_code(code)));
    result.friend_code = code;
}

ChangeResult Engine::add_rule(Rule rule, TimePoint now, RandomSource& random) {
    if (rule.id.empty()) {
        rule.id = new_id("r");
    } else if (find_rule(rule.id)) {
        return refused("there is already a rule called " + rule.id);
    }
    if (const auto* span = std::get_if<Span>(&rule.when); span && span->end <= now) {
        return refused("that session has already ended");
    }
    if (auto error = validate_rule(rule, state_.settings)) return refused(*std::move(error));

    ChangeResult result;
    ensure_friend_code(rule, nullptr, random, result);
    result.outcome = ChangeResult::Outcome::Applied;
    result.rule_id = rule.id;
    result.message = "added";
    state_.rules.push_back(std::move(rule));
    return result;
}

ChangeResult Engine::update_rule(Rule rule, TimePoint now, RandomSource& random) {
    Rule* existing = find_rule_mutable(rule.id);
    if (!existing) return refused("there is no rule called " + rule.id);
    if (auto error = validate_rule(rule, state_.settings)) return refused(*std::move(error));

    ChangeResult result;
    result.rule_id = rule.id;
    ensure_friend_code(rule, existing, random, result);
    if (rule == *existing) {
        result.outcome = ChangeResult::Outcome::Applied;
        result.message = "nothing changed";
        return result;
    }

    if (is_tightening(*existing, rule)) {
        drop_pending_for(rule.id);  // a waiting loosening was based on the old rule
        *existing = std::move(rule);
        result.outcome = ChangeResult::Outcome::Applied;
        result.message = "applied: it only tightens the rule";
        return result;
    }
    if (lock_holds(*existing, now)) {
        const auto end = period_end(*existing, now);
        return refused("this rule is locked" + (end ? " until " + format_time(*end) : std::string{}));
    }

    drop_pending_for(rule.id);
    const Minutes delay = existing->loosen_delay;
    if (delay == Minutes{0}) {
        *existing = std::move(rule);
        result.outcome = ChangeResult::Outcome::Applied;
        result.message = "applied: this rule has no loosen delay";
        return result;
    }
    result.pending_id = new_id("p");
    result.applies_at = now + delay;
    result.outcome = ChangeResult::Outcome::Pending;
    result.message = "this loosens the rule, so it waits until " + format_time(*result.applies_at);
    state_.pending.push_back({result.pending_id, ReplaceRule{std::move(rule)}, *result.applies_at});
    return result;
}

ChangeResult Engine::remove_rule(std::string_view rule_id, TimePoint now) {
    const Rule* existing = find_rule(rule_id);
    if (!existing) return refused("there is no rule called " + std::string(rule_id));
    if (lock_holds(*existing, now)) {
        const auto end = period_end(*existing, now);
        return refused("this rule is locked" + (end ? " until " + format_time(*end) : std::string{}));
    }

    const std::string id(rule_id);
    const Minutes delay = existing->loosen_delay;
    drop_pending_for(id);
    ChangeResult result;
    result.rule_id = id;
    if (delay == Minutes{0}) {
        std::erase_if(state_.rules, [&](const Rule& r) { return r.id == id; });
        forget_rule(id);
        result.outcome = ChangeResult::Outcome::Applied;
        result.message = "removed";
        return result;
    }
    result.pending_id = new_id("p");
    result.applies_at = now + delay;
    result.outcome = ChangeResult::Outcome::Pending;
    result.message = "removing a rule loosens it, so it waits until " + format_time(*result.applies_at);
    state_.pending.push_back({result.pending_id, RemoveRule{id}, *result.applies_at});
    return result;
}

ChangeResult Engine::update_settings(const Settings& settings, TimePoint now) {
    if (auto error = validate_settings(settings)) return refused(*std::move(error));

    ChangeResult result;
    if (settings == state_.settings) {
        result.outcome = ChangeResult::Outcome::Applied;
        result.message = "nothing changed";
        return result;
    }
    const auto is_settings_change = [](const PendingChange& p) {
        return std::holds_alternative<ChangeSettings>(p.change);
    };
    if (is_tightening(state_.settings, settings)) {
        std::erase_if(state_.pending, is_settings_change);
        state_.settings = settings;
        result.outcome = ChangeResult::Outcome::Applied;
        result.message = "applied: it only tightens the settings";
        return result;
    }
    if (any_lock_holds(now)) return refused("settings can't be loosened while a lock holds");

    std::erase_if(state_.pending, is_settings_change);
    result.pending_id = new_id("p");
    result.applies_at = now + kSettingsLoosenDelay;
    result.outcome = ChangeResult::Outcome::Pending;
    result.message = "this loosens the settings, so it waits until " + format_time(*result.applies_at);
    state_.pending.push_back({result.pending_id, ChangeSettings{settings}, *result.applies_at});
    return result;
}

bool Engine::cancel_pending(std::string_view pending_id) {
    return std::erase_if(state_.pending, [&](const PendingChange& p) { return p.id == pending_id; }) > 0;
}

// ---------------------------------------------------------------------------------------------------
// In the moment

ActionResult Engine::continue_after_nudge(std::string_view rule_id, TimePoint now) {
    const Rule* rule = find_rule(rule_id);
    if (!rule) return failed("there is no rule called " + std::string(rule_id));
    if (rule->strength != Strength::Nudge) return failed("only a nudge lets you continue");
    if (!is_enforcing(*rule, now)) return failed("this rule isn't active right now");

    const TimePoint until = now + state_.settings.nudge_continue;
    state_.allowances.push_back({rule->id, now, until});
    return {true, "continue until " + format_time(until), until};
}

ActionResult Engine::start_break(TimePoint now) {
    const std::string today = local_date(now, local_offset_(now));
    if (state_.breaks.date != today) state_.breaks = {today, 0};
    if (state_.breaks.used >= state_.settings.breaks_per_day) return failed("no breaks left today");

    ++state_.breaks.used;
    const TimePoint until = now + state_.settings.break_length;
    state_.allowances.push_back({"", now, until});
    return {true, "break until " + format_time(until), until};
}

ActionResult Engine::request_unlock(std::string_view rule_id, TimePoint now) {
    const Rule* rule = find_rule(rule_id);
    if (!rule || !lock_holds(*rule, now)) return failed("that isn't locked right now");
    if (rule->unlock.kind != UnlockMethod::Kind::Wait) return failed("this lock doesn't unlock by waiting");

    const auto existing = std::ranges::find(state_.unlock_requests, rule_id, &UnlockRequest::rule_id);
    if (existing != state_.unlock_requests.end()) {
        return {true, "already unlocking at " + format_time(existing->due), existing->due};
    }
    const TimePoint due = now + rule->unlock.wait;
    state_.unlock_requests.push_back({rule->id, due});
    return {true, "unlocks at " + format_time(due) + " unless you cancel", due};
}

bool Engine::cancel_unlock(std::string_view rule_id) {
    return std::erase_if(state_.unlock_requests, [&](const UnlockRequest& u) { return u.rule_id == rule_id; }) > 0;
}

std::optional<std::string> Engine::begin_type_unlock(std::string_view rule_id, TimePoint now, RandomSource& random) {
    const Rule* rule = find_rule(rule_id);
    if (!rule || !lock_holds(*rule, now) || rule->unlock.kind != UnlockMethod::Kind::Type) return std::nullopt;

    // The same text until it's typed, so closing the window doesn't deal a new one; but a fresh one if the
    // lock has since been tightened to a longer text.
    const auto existing = std::ranges::find(state_.type_challenges, rule_id, &TypeChallenge::rule_id);
    if (existing != state_.type_challenges.end()) {
        const auto code = normalise_code(existing->text);
        if (code && code->size() == static_cast<std::size_t>(rule->unlock.type_length)) return existing->text;
    }

    std::string text = random_code(random, static_cast<std::size_t>(rule->unlock.type_length), true);
    std::erase_if(state_.type_challenges, [&](const TypeChallenge& c) { return c.rule_id == rule_id; });
    state_.type_challenges.push_back({rule->id, text});
    return text;
}

ActionResult Engine::finish_type_unlock(std::string_view rule_id, std::string_view typed, TimePoint now) {
    const Rule* rule = find_rule(rule_id);
    if (!rule || !lock_holds(*rule, now)) return failed("that isn't locked right now");
    const auto challenge = std::ranges::find(state_.type_challenges, rule_id, &TypeChallenge::rule_id);
    if (challenge == state_.type_challenges.end()) return failed("start the typing challenge first");

    const auto typed_code = normalise_code(typed);
    if (!typed_code || typed_code != normalise_code(challenge->text)) {
        return failed("that doesn't match; check it carefully and try again");
    }
    release(*rule, now);
    return {true, "unlocked", std::nullopt};
}

ActionResult Engine::enter_friend_code(std::string_view rule_id, std::string_view code, TimePoint now) {
    const Rule* rule = find_rule(rule_id);
    if (!rule || !lock_holds(*rule, now)) return failed("that isn't locked right now");
    if (rule->unlock.kind != UnlockMethod::Kind::Friend) return failed("this lock doesn't take a friend code");

    auto attempts = std::ranges::find(state_.friend_attempts, rule_id, &FriendAttempts::rule_id);
    if (attempts == state_.friend_attempts.end()) {
        state_.friend_attempts.push_back({rule->id, 0, TimePoint{}});
        attempts = std::prev(state_.friend_attempts.end());
    }
    if (attempts->retry_after > now) {
        return failed("too many wrong codes; try again at " + format_time(attempts->retry_after),
                      attempts->retry_after);
    }

    bool correct = false;
    if (const auto normalised = normalise_code(code)) {
        const Sha256Digest digest = sha256(rule->unlock.friend_salt + *normalised);
        const auto expected = from_hex(rule->unlock.friend_hash);
        correct = expected && constant_time_equal(digest, *expected);
    }
    if (correct) {
        release(*rule, now);  // also clears the attempt record
        return {true, "unlocked", std::nullopt};
    }

    ++attempts->failures;
    if (attempts->failures % kFriendAttemptsBeforeWait == 0) {
        attempts->retry_after = now + kFriendWaitStep * (attempts->failures / kFriendAttemptsBeforeWait);
        return failed("wrong code; too many tries, so wait until " + format_time(attempts->retry_after),
                      attempts->retry_after);
    }
    return failed("wrong code");
}

FriendStatus Engine::friend_status(std::string_view rule_id, TimePoint now) const {
    const auto attempts = std::ranges::find(state_.friend_attempts, rule_id, &FriendAttempts::rule_id);
    const int failures = attempts == state_.friend_attempts.end() ? 0 : attempts->failures;
    FriendStatus status;
    status.tries_left = kFriendAttemptsBeforeWait - failures % kFriendAttemptsBeforeWait;
    status.next_wait = kFriendWaitStep * (failures / kFriendAttemptsBeforeWait + 1);
    if (attempts != state_.friend_attempts.end() && attempts->retry_after > now) status.retry_after = attempts->retry_after;
    return status;
}

}  // namespace sirenproof::core
