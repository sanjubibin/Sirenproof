#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>

#include "sirenproof/core/state_json.hpp"
#include "test_support.hpp"

using namespace sirenproof::core;
using namespace sirenproof::test;

namespace {

State rich_state() {
    State s;
    s.settings.max_lock = Hours{12};
    s.settings.breaks_per_day = 2;

    Rule work = make_rule("work", {"reddit.com", "youtube.com/shorts", "app:c:\\games\\steam\\", "publisher:valve corp."});
    work.name = "Work hours";
    work.goal = "Ship the report";
    work.allow = {target("youtube.com/watch?v=abc")};
    work.when = *WeeklySchedule::make({{kWeekdays, 9 * 60, 17 * 60}, {kEveryDay, 22 * 60, 2 * 60}});
    work.loosen_delay = Minutes{15};

    Rule exam = make_rule("exam", {"youtube.com"}, Strength::Lock);
    exam.when = Span{at("2026-11-09T08:00:00Z"), at("2026-11-09T18:00:00Z")};
    exam.unlock = {UnlockMethod::Kind::Friend, Minutes{0}, 0, std::string(32, 'a'), std::string(64, 'b')};
    exam.breaks_allowed = true;

    Rule gentle = make_rule("gentle", {"x.com"}, Strength::Nudge);
    gentle.loosen_delay = Minutes{2};

    s.rules = {work, exam, gentle};

    Rule looser = work;
    looser.block.pop_back();
    Settings looser_settings = s.settings;
    looser_settings.breaks_per_day = 4;
    s.pending = {{"p1", ReplaceRule{looser}, at("2026-11-02T09:15:00Z")},
                 {"p2", RemoveRule{"gentle"}, at("2026-11-02T09:02:00Z")},
                 {"p3", ChangeSettings{looser_settings}, at("2026-11-02T09:10:00Z")}};
    s.allowances = {{"gentle", at("2026-11-02T09:00:00Z"), at("2026-11-02T09:05:00Z")},
                    {"", at("2026-11-02T09:00:00Z"), at("2026-11-02T09:05:00Z")}};
    s.releases = {{"exam", at("2026-11-09T10:00:00Z"), at("2026-11-09T18:00:00Z")}};
    s.unlock_requests = {{"exam", at("2026-11-09T11:00:00Z")}};
    s.type_challenges = {{"exam", "abcde-fghjk"}};
    s.friend_attempts = {{"exam", 5, at("2026-11-09T10:10:00Z")}};
    s.breaks = {"2026-11-02", 1};
    s.clock = ClockCheckpoint{at("2026-11-02T09:00:00Z"), at("2026-11-02T10:00:00Z"), Millis{123'456},
                              18'000'000'000'000'000'000u};
    s.next_id = 42;
    return s;
}

std::string error_for(std::string_view json_text) {
    const auto state = parse_state(json_text);
    REQUIRE_FALSE(state.has_value());
    return state.error();
}

}  // namespace

TEST_CASE("A full state survives a save and load unchanged", "[json]") {
    const State original = rich_state();
    const std::string text = serialise_state(original);
    const auto loaded = parse_state(text);
    REQUIRE(loaded.has_value());
    CHECK(*loaded == original);
    CHECK(serialise_state(*loaded) == text);
}

TEST_CASE("The example state file loads", "[json]") {
    std::ifstream in(SIRENPROOF_EXAMPLES_DIR "/state.example.json");
    REQUIRE(in);
    std::ostringstream text;
    text << in.rdbuf();
    const auto state = parse_state(text.str());
    INFO((state ? std::string("ok") : state.error()));
    REQUIRE(state.has_value());
    CHECK(state->rules.size() == 3);
}

TEST_CASE("Optional fields take sensible defaults", "[json]") {
    const auto state = parse_state(R"({"version": 1, "rules": [{"id": "a", "block": ["reddit.com"]}]})");
    REQUIRE(state.has_value());
    const Rule& rule = state->rules.at(0);
    CHECK(rule.strength == Strength::Block);
    CHECK(rule.loosen_delay == Minutes{10});
    CHECK(std::holds_alternative<Always>(rule.when));
    CHECK(state->settings == Settings{});
}

TEST_CASE("Loading is strict and says where the problem is", "[json]") {
    CHECK(error_for("{").starts_with("not valid JSON"));
    CHECK(error_for("{}") == "version: missing");
    CHECK(error_for(R"({"version": 2})").starts_with("version:"));
    CHECK(error_for(R"({"version": 1, "rulez": []})") == "rulez: unknown field");
    CHECK(error_for(R"({"version": 1, "rules": [{"id": "a", "block": ["reddit.com"], "strenght": "lock"}]})") ==
          "rules[0].strenght: unknown field");
    CHECK(error_for(R"({"version": 1, "rules": [{"id": "a", "block": ["reddit.com", "not a target"]}]})") ==
          "rules[0].block[1]: not a valid target");
    CHECK(error_for(R"({"version": 1, "rules": [{"id": 5, "block": []}]})") == "rules[0].id: expected text");
    CHECK(error_for(R"({"version": 1, "rules": [{"id": "a", "block": ["x.com"]}, {"id": "a", "block": ["y.com"]}]})") ==
          "rules[1].id: used twice");
    CHECK(error_for(R"({"version": 1, "rules": [{"id": "a", "block": ["x.com"], "when": {"from": "soon", "to": "later"}}]})")
              .starts_with("rules[0].when.from:"));
}

TEST_CASE("Loading re-checks the safety rails", "[json][safety]") {
    const std::string protected_target =
        error_for(R"({"version": 1, "rules": [{"id": "a", "block": ["app:explorer.exe"]}]})");
    CHECK(protected_target.starts_with("rules[0]:"));
    CHECK(protected_target.find("explorer.exe") != std::string::npos);

    const std::string endless_lock =
        error_for(R"({"version": 1, "rules": [{"id": "a", "block": ["x.com"], "strength": "lock"}]})");
    CHECK(endless_lock.find("needs an end") != std::string::npos);
}

TEST_CASE("Lowering the lock cap never makes an existing state unreadable", "[json]") {
    const auto state = parse_state(R"({
        "version": 1,
        "settings": {"max_lock_hours": 1},
        "rules": [{"id": "exam", "block": ["x.com"], "strength": "lock",
                   "when": {"from": "2026-11-09T08:00:00Z", "to": "2026-11-09T18:00:00Z"}}]
    })");
    CHECK(state.has_value());
}
