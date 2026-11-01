#include <catch2/catch_test_macros.hpp>

#include "sirenproof/core/rule.hpp"
#include "test_support.hpp"

using namespace sirenproof::core;
using namespace sirenproof::test;

namespace {

Rule lock_rule(When when) {
    Rule rule = make_rule("exam", {"youtube.com"}, Strength::Lock);
    rule.when = std::move(when);
    return rule;
}

}  // namespace

TEST_CASE("validate_rule accepts a plain rule", "[rule]") {
    CHECK_FALSE(validate_rule(make_rule("work", {"reddit.com", "app:steam.exe"}), Settings{}));
}

TEST_CASE("validate_rule enforces the basic shape", "[rule]") {
    CHECK(validate_rule(make_rule("", {"reddit.com"}), Settings{}));
    CHECK(validate_rule(make_rule("Has Space", {"reddit.com"}), Settings{}));
    CHECK(validate_rule(make_rule("empty", {}), Settings{}));

    Rule slow = make_rule("slow", {"reddit.com"});
    slow.loosen_delay = std::chrono::days{8};
    CHECK(validate_rule(slow, Settings{}));

    Rule backwards = make_rule("backwards", {"reddit.com"});
    backwards.when = Span{at("2026-11-02T10:00:00Z"), at("2026-11-02T09:00:00Z")};
    CHECK(validate_rule(backwards, Settings{}));

    Rule not_a_lock = make_rule("nudge", {"reddit.com"}, Strength::Nudge);
    not_a_lock.unlock.kind = UnlockMethod::Kind::Wait;
    not_a_lock.unlock.wait = Minutes{30};
    CHECK(validate_rule(not_a_lock, Settings{}));
}

TEST_CASE("validate_rule refuses protected targets", "[rule][safety]") {
    const auto error = validate_rule(make_rule("bad", {"app:explorer.exe"}), Settings{});
    REQUIRE(error);
    CHECK(error->find("explorer.exe") != std::string::npos);
}

TEST_CASE("A lock must end, and within the cap", "[rule][safety]") {
    const Settings settings;  // 24 hour cap
    CHECK(validate_rule(lock_rule(Always{}), settings));

    const TimePoint start = at("2026-11-02T08:00:00Z");
    CHECK_FALSE(validate_rule(lock_rule(Span{start, start + Hours{24}}), settings));
    CHECK(validate_rule(lock_rule(Span{start, start + Hours{25}}), settings));

    CHECK_FALSE(validate_rule(lock_rule(schedule(kWeekdays, 9 * 60, 17 * 60)), settings));
    CHECK(validate_rule(lock_rule(schedule(kEveryDay, 0, kMinutesPerDay)), settings));  // never switches off
}

TEST_CASE("Unlock methods are range-checked", "[rule]") {
    const TimePoint start = at("2026-11-02T08:00:00Z");
    Rule rule = lock_rule(Span{start, start + Hours{2}});

    rule.unlock = {UnlockMethod::Kind::Wait, Minutes{0}, 0, "", ""};
    CHECK(validate_rule(rule, Settings{}));
    rule.unlock = {UnlockMethod::Kind::Wait, Minutes{60}, 0, "", ""};
    CHECK_FALSE(validate_rule(rule, Settings{}));

    rule.unlock = {UnlockMethod::Kind::Type, Minutes{0}, 10, "", ""};
    CHECK(validate_rule(rule, Settings{}));
    rule.unlock = {UnlockMethod::Kind::Type, Minutes{0}, 200, "", ""};
    CHECK_FALSE(validate_rule(rule, Settings{}));

    rule.unlock = {UnlockMethod::Kind::Friend, Minutes{0}, 0, "", ""};  // not generated yet: fine
    CHECK_FALSE(validate_rule(rule, Settings{}));
    rule.unlock = {UnlockMethod::Kind::Friend, Minutes{0}, 0, "abcd", "1234"};  // damaged
    CHECK(validate_rule(rule, Settings{}));
}

TEST_CASE("is_tightening: only stricter changes count", "[rule]") {
    const Rule base = make_rule("work", {"reddit.com", "x.com"});

    Rule more = base;
    more.block.push_back(target("youtube.com/shorts"));
    CHECK(is_tightening(base, more));
    CHECK_FALSE(is_tightening(more, base));

    Rule with_exception = base;
    with_exception.allow.push_back(target("reddit.com/r/cpp"));
    CHECK_FALSE(is_tightening(base, with_exception));
    CHECK(is_tightening(with_exception, base));

    Rule stronger = base;
    stronger.strength = Strength::Lock;
    CHECK(is_tightening(base, stronger));
    CHECK_FALSE(is_tightening(stronger, base));

    Rule slower = base;
    slower.loosen_delay = Minutes{60};
    CHECK(is_tightening(base, slower));
    CHECK_FALSE(is_tightening(slower, base));

    Rule renamed = base;
    renamed.name = "Focus";
    renamed.goal = "Write the essay";
    CHECK(is_tightening(base, renamed));
}

TEST_CASE("is_tightening compares when a rule applies", "[rule]") {
    Rule always = make_rule("r", {"reddit.com"});
    Rule work = always;
    work.when = schedule(kWeekdays, 9 * 60, 17 * 60);
    Rule longer = always;
    longer.when = schedule(kWeekdays, 8 * 60, 18 * 60);

    CHECK_FALSE(is_tightening(always, work));
    CHECK(is_tightening(work, always));
    CHECK(is_tightening(work, longer));
    CHECK_FALSE(is_tightening(longer, work));

    const TimePoint t = at("2026-11-02T08:00:00Z");
    Rule session = always;
    session.when = Span{t, t + Hours{2}};
    Rule extended = always;
    extended.when = Span{t, t + Hours{3}};
    CHECK(is_tightening(session, extended));
    CHECK_FALSE(is_tightening(extended, session));
}

TEST_CASE("is_tightening compares unlock methods and breaks", "[rule]") {
    const TimePoint t = at("2026-11-02T08:00:00Z");
    Rule wait30 = lock_rule(Span{t, t + Hours{2}});
    wait30.unlock = {UnlockMethod::Kind::Wait, Minutes{30}, 0, "", ""};
    Rule wait60 = wait30;
    wait60.unlock.wait = Minutes{60};
    Rule no_unlock = wait30;
    no_unlock.unlock = {};
    Rule typed = wait30;
    typed.unlock = {UnlockMethod::Kind::Type, Minutes{0}, 200, "", ""};

    CHECK(is_tightening(wait30, wait60));
    CHECK_FALSE(is_tightening(wait60, wait30));
    CHECK(is_tightening(wait30, no_unlock));
    CHECK_FALSE(is_tightening(no_unlock, wait30));
    CHECK_FALSE(is_tightening(wait30, typed));

    Rule breaks = wait30;
    breaks.breaks_allowed = true;
    CHECK_FALSE(is_tightening(wait30, breaks));
    CHECK(is_tightening(breaks, wait30));
}

TEST_CASE("Settings: stricter values tighten, and values are range-checked", "[rule]") {
    const Settings base;
    Settings fewer_breaks = base;
    fewer_breaks.breaks_per_day = 1;
    Settings longer_cap = base;
    longer_cap.max_lock = Hours{48};
    Settings longer_continue = base;
    longer_continue.nudge_continue = Minutes{20};

    CHECK(is_tightening(base, fewer_breaks));
    CHECK(is_tightening(base, longer_cap));
    CHECK_FALSE(is_tightening(base, longer_continue));
    CHECK_FALSE(is_tightening(fewer_breaks, base));

    CHECK_FALSE(validate_settings(base));
    Settings bad = base;
    bad.max_lock = Hours{0};
    CHECK(validate_settings(bad));
    bad = base;
    bad.nudge_pause = Seconds{1};
    CHECK(validate_settings(bad));
}
