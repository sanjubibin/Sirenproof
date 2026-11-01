#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "sirenproof/core/clock.hpp"
#include "sirenproof/core/policy.hpp"
#include "test_support.hpp"

using namespace sirenproof::core;
using namespace sirenproof::test;
using Outcome = ChangeResult::Outcome;

namespace {

const TimePoint kMonday9 = at("2026-11-02T09:00:00Z");  // a Monday; the tests run on UTC local time

Url url(std::string_view text) {
    auto u = Url::parse(text);
    REQUIRE(u.has_value());
    return *u;
}

AppIdentity program(std::string path) { return AppIdentity{std::move(path), "", "", ""}; }

Rule session(std::string id, std::initializer_list<std::string_view> block, Strength strength, TimePoint start,
             Hours length) {
    Rule rule = make_rule(std::move(id), block, strength);
    rule.when = Span{start, start + length};
    return rule;
}

struct Fixture {
    State state;
    Engine engine{state, utc};
    CountingRandom random;

    ChangeResult add(Rule rule, TimePoint now = kMonday9) {
        ChangeResult result = engine.add_rule(std::move(rule), now, random);
        REQUIRE(result.outcome == Outcome::Applied);
        return result;
    }

    Action web(std::string_view address, TimePoint now = kMonday9) const {
        return engine.decide(url(address), now).action;
    }
};

}  // namespace

// ---------------------------------------------------------------------------------------------------
// Deciding

TEST_CASE_METHOD(Fixture, "A domain rule blocks the site and its subdomains", "[policy]") {
    add(make_rule("work", {"reddit.com"}));
    const Verdict verdict = engine.decide(url("https://old.reddit.com/r/all"), kMonday9);
    CHECK(verdict.action == Action::Block);
    CHECK(verdict.rule_id == "work");
    CHECK(web("notreddit.com") == Action::Allow);
}

TEST_CASE_METHOD(Fixture, "An exception only applies within its own rule", "[policy]") {
    Rule youtube = make_rule("yt", {"youtube.com"});
    youtube.allow = {target("youtube.com/watch")};
    add(youtube);
    CHECK(web("youtube.com/watch?v=1") == Action::Allow);
    CHECK(web("youtube.com/") == Action::Block);

    add(make_rule("strict", {"youtube.com/watch"}, Strength::Nudge));
    const Verdict verdict = engine.decide(url("youtube.com/watch?v=1"), kMonday9);
    CHECK(verdict.action == Action::Nudge);
    CHECK(verdict.rule_id == "strict");
}

TEST_CASE_METHOD(Fixture, "The strictest matching rule decides", "[policy]") {
    add(make_rule("gentle", {"reddit.com"}, Strength::Nudge));
    add(make_rule("firm", {"reddit.com"}, Strength::Block));
    const Verdict verdict = engine.decide(url("reddit.com"), kMonday9);
    CHECK(verdict.action == Action::Block);
    CHECK(verdict.rule_id == "firm");
}

TEST_CASE_METHOD(Fixture, "Programs are decided by program rules only", "[policy]") {
    add(make_rule("games", {"app:steam.exe", "reddit.com"}));
    CHECK(engine.decide(program("C:\\Steam\\steam.exe"), kMonday9).action == Action::Block);
    CHECK(engine.decide(program("C:\\Tools\\notepad++.exe"), kMonday9).action == Action::Allow);
}

TEST_CASE_METHOD(Fixture, "A weekly rule applies inside its windows and says when it ends", "[policy]") {
    Rule work = make_rule("work", {"reddit.com"});
    work.when = schedule(kWeekdays, 9 * 60, 17 * 60);
    add(work);

    const Verdict verdict = engine.decide(url("reddit.com"), at("2026-11-02T10:00:00Z"));
    CHECK(verdict.action == Action::Block);
    CHECK(verdict.until == at("2026-11-02T17:00:00Z"));
    CHECK(web("reddit.com", at("2026-11-02T18:00:00Z")) == Action::Allow);
    CHECK(web("reddit.com", at("2026-11-07T10:00:00Z")) == Action::Allow);  // Saturday
}

TEST_CASE_METHOD(Fixture, "The plan tells each layer what to enforce", "[policy]") {
    Rule work = make_rule("work", {"reddit.com", "youtube.com", "app:steam.exe"});
    work.allow = {target("youtube.com/watch")};
    add(work);
    add(make_rule("gentle", {"x.com"}, Strength::Nudge));

    const Plan plan = engine.plan(kMonday9);
    CHECK(plan.active_rules == std::vector<std::string>{"work", "gentle"});
    // youtube.com has an exception and x.com is only a nudge, so neither can go in the hosts file.
    REQUIRE(plan.hosts.size() == 1);
    CHECK(plan.hosts[0].str() == "reddit.com");

    const auto has = [](const std::vector<PlanEntry>& entries, std::string_view target_text, Action action) {
        return std::ranges::any_of(entries, [&](const PlanEntry& e) { return e.target == target_text && e.action == action; });
    };
    CHECK(has(plan.apps, "app:steam.exe", Action::Block));
    CHECK(has(plan.web, "youtube.com/watch", Action::Allow));
    CHECK(has(plan.web, "x.com", Action::Nudge));
    CHECK_FALSE(plan.next_change.has_value());  // nothing here ever switches
}

// ---------------------------------------------------------------------------------------------------
// Changing rules: tighten now, loosen later

TEST_CASE_METHOD(Fixture, "Adding a rule applies at once, with checks", "[policy][changes]") {
    const ChangeResult added = engine.add_rule(make_rule("", {"reddit.com"}), kMonday9, random);
    CHECK(added.outcome == Outcome::Applied);
    CHECK(added.rule_id == "r1");
    CHECK(engine.find_rule("r1") != nullptr);

    CHECK(engine.add_rule(make_rule("r1", {"x.com"}), kMonday9, random).outcome == Outcome::Refused);
    const ChangeResult protected_target = engine.add_rule(make_rule("bad", {"app:explorer.exe"}), kMonday9, random);
    CHECK(protected_target.outcome == Outcome::Refused);
    CHECK(protected_target.message.find("explorer.exe") != std::string::npos);

    Rule over = make_rule("over", {"reddit.com"});
    over.when = Span{kMonday9 - Hours{2}, kMonday9 - Hours{1}};
    CHECK(engine.add_rule(over, kMonday9, random).outcome == Outcome::Refused);
}

TEST_CASE_METHOD(Fixture, "Tightening applies at once; loosening waits out the delay", "[policy][changes]") {
    add(make_rule("work", {"reddit.com", "x.com"}));

    Rule tighter = *engine.find_rule("work");
    tighter.block.push_back(target("youtube.com"));
    CHECK(engine.update_rule(tighter, kMonday9, random).outcome == Outcome::Applied);
    CHECK(web("youtube.com") == Action::Block);

    Rule looser = tighter;
    looser.block = {target("reddit.com")};
    const ChangeResult change = engine.update_rule(looser, kMonday9, random);
    REQUIRE(change.outcome == Outcome::Pending);
    CHECK(change.applies_at == kMonday9 + Minutes{10});

    engine.tick(kMonday9 + Minutes{9});
    CHECK(web("x.com", kMonday9 + Minutes{9}) == Action::Block);

    engine.tick(kMonday9 + Minutes{10});
    CHECK(web("x.com", kMonday9 + Minutes{10}) == Action::Allow);
    CHECK(state.pending.empty());
}

TEST_CASE_METHOD(Fixture, "A waiting loosening can be cancelled", "[policy][changes]") {
    add(make_rule("work", {"reddit.com", "x.com"}));
    Rule looser = *engine.find_rule("work");
    looser.block = {target("reddit.com")};
    const ChangeResult change = engine.update_rule(looser, kMonday9, random);
    REQUIRE(change.outcome == Outcome::Pending);

    CHECK(engine.cancel_pending(change.pending_id));
    engine.tick(kMonday9 + Minutes{30});
    CHECK(web("x.com", kMonday9 + Minutes{30}) == Action::Block);
}

TEST_CASE_METHOD(Fixture, "A rule without a loosen delay changes at once", "[policy][changes]") {
    Rule quick = make_rule("quick", {"reddit.com", "x.com"});
    quick.loosen_delay = Minutes{0};
    add(quick);
    quick.block = {target("reddit.com")};
    CHECK(engine.update_rule(quick, kMonday9, random).outcome == Outcome::Applied);
    CHECK(web("x.com") == Action::Allow);
}

TEST_CASE_METHOD(Fixture, "Removing a rule waits out its delay too", "[policy][changes]") {
    add(make_rule("work", {"reddit.com"}));
    CHECK(engine.remove_rule("work", kMonday9).outcome == Outcome::Pending);
    engine.tick(kMonday9 + Minutes{5});
    CHECK(engine.find_rule("work") != nullptr);
    engine.tick(kMonday9 + Minutes{10});
    CHECK(engine.find_rule("work") == nullptr);
}

TEST_CASE_METHOD(Fixture, "Settings tighten at once and loosen after a delay", "[policy][changes]") {
    Settings fewer = state.settings;
    fewer.breaks_per_day = 1;
    CHECK(engine.update_settings(fewer, kMonday9).outcome == Outcome::Applied);
    CHECK(state.settings.breaks_per_day == 1);

    Settings more = fewer;
    more.breaks_per_day = 5;
    CHECK(engine.update_settings(more, kMonday9).outcome == Outcome::Pending);
    CHECK(state.settings.breaks_per_day == 1);
    engine.tick(kMonday9 + Engine::kSettingsLoosenDelay);
    CHECK(state.settings.breaks_per_day == 5);
}

// ---------------------------------------------------------------------------------------------------
// Locks

TEST_CASE_METHOD(Fixture, "A lock refuses loosening while it holds, but accepts tightening", "[policy][lock]") {
    add(session("exam", {"youtube.com"}, Strength::Lock, kMonday9, Hours{3}));
    const TimePoint during = kMonday9 + Hours{1};

    Rule looser = *engine.find_rule("exam");
    looser.block = {target("reddit.com")};
    const ChangeResult refused = engine.update_rule(looser, during, random);
    CHECK(refused.outcome == Outcome::Refused);
    CHECK(refused.message.find("locked") != std::string::npos);
    CHECK(engine.remove_rule("exam", during).outcome == Outcome::Refused);

    Settings more_breaks = state.settings;
    more_breaks.breaks_per_day = 10;
    CHECK(engine.update_settings(more_breaks, during).outcome == Outcome::Refused);

    Rule tighter = *engine.find_rule("exam");
    tighter.block.push_back(target("reddit.com"));
    CHECK(engine.update_rule(tighter, during, random).outcome == Outcome::Applied);
    CHECK(web("reddit.com", during) == Action::Block);
}

TEST_CASE_METHOD(Fixture, "A lock ends on time and the finished session is cleared away", "[policy][lock]") {
    add(session("exam", {"youtube.com"}, Strength::Lock, kMonday9, Hours{2}));
    CHECK(web("youtube.com", kMonday9 + Minutes{119}) == Action::Block);

    engine.tick(kMonday9 + Hours{2});
    CHECK(engine.find_rule("exam") == nullptr);
    CHECK(web("youtube.com", kMonday9 + Hours{2}) == Action::Allow);
}

TEST_CASE_METHOD(Fixture, "A waiting loosening holds off while a scheduled lock is on", "[policy][lock]") {
    Rule nights = make_rule("nights", {"app:steam.exe", "app:dota2.exe"}, Strength::Lock);
    nights.when = schedule(kEveryDay, 22 * 60, 6 * 60);
    add(nights);

    // Asked for at 21:55, outside the lock, so it may wait out the delay...
    Rule looser = nights;
    looser.block = {target("app:dota2.exe")};
    const ChangeResult change = engine.update_rule(looser, at("2026-11-02T21:55:00Z"), random);
    REQUIRE(change.outcome == Outcome::Pending);

    // ...but at 22:05 the lock is on, so it keeps waiting until the lock is off again.
    engine.tick(at("2026-11-02T22:05:00Z"));
    CHECK(state.pending.size() == 1);
    CHECK(engine.decide(program("C:\\Steam\\steam.exe"), at("2026-11-02T23:00:00Z")).action == Action::Block);

    engine.tick(at("2026-11-03T06:00:00Z"));
    CHECK(state.pending.empty());
    CHECK(engine.find_rule("nights")->block.size() == 1);
}

TEST_CASE_METHOD(Fixture, "Unlocking by waiting releases the lock when the wait is over", "[policy][unlock]") {
    Rule exam = session("exam", {"youtube.com"}, Strength::Lock, kMonday9, Hours{4});
    exam.unlock = {UnlockMethod::Kind::Wait, Minutes{60}, 0, "", ""};
    add(exam);

    const ActionResult asked = engine.request_unlock("exam", kMonday9 + Minutes{30});
    CHECK(asked.ok);
    CHECK(asked.until == kMonday9 + Minutes{90});
    CHECK(engine.request_unlock("exam", kMonday9 + Minutes{40}).until == kMonday9 + Minutes{90});  // no restart

    engine.tick(kMonday9 + Minutes{89});
    CHECK(web("youtube.com", kMonday9 + Minutes{89}) == Action::Block);
    engine.tick(kMonday9 + Minutes{90});
    CHECK(web("youtube.com", kMonday9 + Minutes{90}) == Action::Allow);
    CHECK(web("youtube.com", kMonday9 + Hours{3}) == Action::Allow);  // released until the session's end
}

TEST_CASE_METHOD(Fixture, "A waiting unlock can be called off", "[policy][unlock]") {
    Rule exam = session("exam", {"youtube.com"}, Strength::Lock, kMonday9, Hours{4});
    exam.unlock = {UnlockMethod::Kind::Wait, Minutes{60}, 0, "", ""};
    add(exam);
    REQUIRE(engine.request_unlock("exam", kMonday9).ok);
    CHECK(engine.cancel_unlock("exam"));
    engine.tick(kMonday9 + Minutes{61});
    CHECK(web("youtube.com", kMonday9 + Minutes{61}) == Action::Block);
}

TEST_CASE_METHOD(Fixture, "Unlocking by typing needs the exact text", "[policy][unlock]") {
    Rule exam = session("exam", {"youtube.com"}, Strength::Lock, kMonday9, Hours{4});
    exam.unlock = {UnlockMethod::Kind::Type, Minutes{0}, 30, "", ""};
    add(exam);

    CHECK_FALSE(engine.finish_type_unlock("exam", "anything", kMonday9).ok);  // no challenge yet

    const auto text = engine.begin_type_unlock("exam", kMonday9, random);
    REQUIRE(text.has_value());
    CHECK(text->size() == 35);  // 30 characters in groups of five

    CHECK_FALSE(engine.finish_type_unlock("exam", "not it", kMonday9).ok);
    CHECK(web("youtube.com") == Action::Block);

    std::string shouted = *text;
    for (char& c : shouted) c = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    CHECK(engine.finish_type_unlock("exam", shouted, kMonday9).ok);
    CHECK(web("youtube.com") == Action::Allow);
}

TEST_CASE_METHOD(Fixture, "A friend's code unlocks; wrong guesses slow down further guesses", "[policy][unlock]") {
    Rule exam = session("exam", {"youtube.com"}, Strength::Lock, kMonday9, Hours{4});
    exam.unlock.kind = UnlockMethod::Kind::Friend;
    const std::string code = add(exam).friend_code;
    REQUIRE(code.size() == 17);

    // Only a salted hash is kept.
    const Rule* stored = engine.find_rule("exam");
    CHECK(stored->unlock.friend_salt.size() == 32);
    CHECK(stored->unlock.friend_hash.size() == 64);
    CHECK(stored->unlock.friend_hash.find(code) == std::string::npos);

    for (int i = 0; i < 4; ++i) CHECK_FALSE(engine.enter_friend_code("exam", "00000-00000-00000", kMonday9).ok);
    const ActionResult fifth = engine.enter_friend_code("exam", "00000-00000-00000", kMonday9);
    CHECK_FALSE(fifth.ok);
    CHECK(fifth.until == kMonday9 + Minutes{10});

    CHECK_FALSE(engine.enter_friend_code("exam", code, kMonday9 + Minutes{5}).ok);  // still waiting

    std::string typed = code;  // spaces instead of dashes and capitals are fine
    for (char& c : typed) c = c == '-' ? ' ' : ((c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c);
    CHECK(engine.enter_friend_code("exam", typed, kMonday9 + Minutes{10}).ok);
    CHECK(web("youtube.com", kMonday9 + Minutes{10}) == Action::Allow);
}

// ---------------------------------------------------------------------------------------------------
// Breaks and nudges

TEST_CASE_METHOD(Fixture, "Breaks pause blocks, but a lock only if it allows breaks", "[policy][breaks]") {
    add(make_rule("work", {"reddit.com"}));
    add(session("exam", {"youtube.com"}, Strength::Lock, kMonday9, Hours{3}));
    Rule study = session("study", {"x.com"}, Strength::Lock, kMonday9, Hours{3});
    study.breaks_allowed = true;
    add(study);

    const TimePoint t = kMonday9 + Minutes{1};
    const ActionResult pause = engine.start_break(t);
    REQUIRE(pause.ok);
    CHECK(pause.until == t + Minutes{5});
    CHECK(web("reddit.com", t) == Action::Allow);
    CHECK(web("youtube.com", t) == Action::Block);
    CHECK(web("x.com", t) == Action::Allow);
    CHECK(web("reddit.com", t + Minutes{5}) == Action::Block);
}

TEST_CASE_METHOD(Fixture, "Breaks are limited per local day", "[policy][breaks]") {
    for (int i = 0; i < 3; ++i) CHECK(engine.start_break(kMonday9 + Hours{i}).ok);
    CHECK_FALSE(engine.start_break(kMonday9 + Hours{4}).ok);
    CHECK(engine.start_break(at("2026-11-03T09:00:00Z")).ok);  // a new day
}

TEST_CASE_METHOD(Fixture, "Continuing after a nudge lasts a few minutes, for nudges only", "[policy][nudge]") {
    add(make_rule("gentle", {"reddit.com"}, Strength::Nudge));
    add(make_rule("firm", {"x.com"}));

    const ActionResult go = engine.continue_after_nudge("gentle", kMonday9);
    REQUIRE(go.ok);
    CHECK(web("reddit.com", kMonday9 + Minutes{4}) == Action::Allow);
    CHECK(web("reddit.com", kMonday9 + Minutes{5}) == Action::Nudge);
    CHECK_FALSE(engine.continue_after_nudge("firm", kMonday9).ok);
}

// ---------------------------------------------------------------------------------------------------
// Clock tampering

TEST_CASE_METHOD(Fixture, "time_change_is_safe only allows jumps that loosen nothing", "[policy][tamper]") {
    CHECK(engine.time_change_is_safe(kMonday9, kMonday9 + Hours{5}));  // nothing to protect

    Rule work = make_rule("work", {"reddit.com"});
    work.when = schedule(kWeekdays, 9 * 60, 17 * 60);
    add(work);
    CHECK_FALSE(engine.time_change_is_safe(at("2026-11-02T10:00:00Z"), at("2026-11-02T18:00:00Z")));
    CHECK(engine.time_change_is_safe(at("2026-11-02T08:00:00Z"), at("2026-11-02T10:00:00Z")));  // into a block
    CHECK(engine.time_change_is_safe(at("2026-11-02T18:00:00Z"), at("2026-11-02T19:00:00Z")));

    add(session("exam", {"youtube.com"}, Strength::Lock, at("2026-11-02T20:00:00Z"), Hours{2}));
    CHECK_FALSE(engine.time_change_is_safe(at("2026-11-02T20:30:00Z"), at("2026-11-02T23:00:00Z")));
    CHECK_FALSE(engine.time_change_is_safe(at("2026-11-02T20:30:00Z"), at("2026-11-02T19:00:00Z")));
}

TEST_CASE_METHOD(Fixture, "Jumping the clock past a waiting change is not safe", "[policy][tamper]") {
    add(make_rule("work", {"reddit.com", "x.com"}));
    Rule looser = *engine.find_rule("work");
    looser.block = {target("reddit.com")};
    REQUIRE(engine.update_rule(looser, kMonday9, random).outcome == Outcome::Pending);

    CHECK(engine.time_change_is_safe(kMonday9, kMonday9 + Minutes{5}));
    CHECK_FALSE(engine.time_change_is_safe(kMonday9, kMonday9 + Hours{1}));
}

TEST_CASE_METHOD(Fixture, "Moving the Windows clock forward doesn't end a lock", "[policy][tamper]") {
    add(session("exam", {"youtube.com"}, Strength::Lock, kMonday9, Hours{2}));
    const TimeChangeArbiter arbiter = [&](TimePoint from, TimePoint to) { return engine.time_change_is_safe(from, to); };

    TrustedClock clock(ClockReading{kMonday9, Millis{0}, 1});
    // A minute in, the clock is moved three hours ahead.
    clock.update(ClockReading{kMonday9 + Hours{3}, std::chrono::minutes{1}, 1}, arbiter);
    CHECK(clock.last_jump_ignored());
    CHECK(clock.now() == kMonday9 + Minutes{1});
    CHECK(web("youtube.com", clock.now()) == Action::Block);

    // Once two real hours have passed the lock is over, so the clock's time can be taken over.
    clock.update(ClockReading{kMonday9 + Hours{5}, Hours{2}, 1}, arbiter);
    CHECK(clock.now() == kMonday9 + Hours{5});
    CHECK(clock.offset() == Millis{0});
}
