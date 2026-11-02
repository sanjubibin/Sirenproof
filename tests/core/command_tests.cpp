#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "sirenproof/core/command.hpp"
#include "sirenproof/core/target.hpp"

using namespace sirenproof::core;
using Strings = std::vector<std::string>;

TEST_CASE("Ready-made lists and well-known names hold valid targets", "[command]") {
    REQUIRE(ready_lists().size() == 3);
    for (const ReadyList& list : ready_lists()) {
        CHECK(find_ready_list(list.id) == &list);
        for (const std::string_view t : list.targets) CHECK(parse_target(t));
    }
    CHECK_FALSE(find_ready_list("nothing"));
    for (const std::string_view name : {"youtube", "reddit", "x", "twitter", "steam", "discord", "twitch"}) {
        CHECK_FALSE(named_targets(name).empty());
        for (const std::string_view t : named_targets(name)) CHECK(parse_target(t));
    }
    CHECK(named_targets("notaname").empty());
}

TEST_CASE("block and nudge take targets and an optional length", "[command]") {
    const auto block = parse_command("block reddit.com 25m");
    REQUIRE(block);
    CHECK(block->kind == Command::Kind::Block);
    CHECK(block->strength == Strength::Block);
    CHECK(block->targets == Strings{"reddit.com"});
    CHECK(block->length == Minutes{25});

    // The command line's own spelling works too.
    const auto spelled = parse_command("block reddit.com --for 25m");
    REQUIRE(spelled);
    CHECK(spelled->length == Minutes{25});

    const auto always = parse_command("  Block   YouTube.com/Shorts  app:steam.exe ");
    REQUIRE(always);
    CHECK(always->targets == Strings{"youtube.com/shorts", "app:steam.exe"});
    CHECK_FALSE(always->length);

    const auto nudge = parse_command("nudge twitch 1h");
    REQUIRE(nudge);
    CHECK(nudge->strength == Strength::Nudge);
    CHECK(nudge->targets == Strings{"twitch.tv"});
    CHECK(parse_command("block twitch.tv --nudge")->strength == Strength::Nudge);
}

TEST_CASE("Names and ready-made lists stand for their targets, once each", "[command]") {
    const auto named = parse_command("block reddit discord reddit.com 2h");
    REQUIRE(named);
    CHECK(named->targets == Strings{"reddit.com", "app:discord.exe", "discord.com"});

    const auto lists = parse_command("lock video social video 2h --unlock type:200");
    REQUIRE(lists);
    CHECK(lists->kind == Command::Kind::Lock);
    CHECK(lists->lists == Strings{"video", "social"});
    CHECK(lists->targets.empty());
    CHECK(lists->unlock == "type");
}

TEST_CASE("A lock needs a length; block --lock is a lock", "[command]") {
    const auto lock = parse_command("lock youtube 2h");
    REQUIRE(lock);
    CHECK(lock->kind == Command::Kind::Lock);
    CHECK(lock->strength == Strength::Lock);
    CHECK(lock->targets == Strings{"youtube.com"});
    CHECK(lock->length == Minutes{120});
    CHECK(lock->unlock.empty());

    const auto spelled = parse_command("block reddit.com --lock --for 2h --unlock friend");
    REQUIRE(spelled);
    CHECK(spelled->kind == Command::Kind::Lock);
    CHECK(spelled->unlock == "friend");

    CHECK(parse_command("lock 45m"));  // just the length; the screen keeps what's chosen
    CHECK(parse_command("lock youtube").error().message.find("needs a length") != std::string::npos);
}

TEST_CASE("Other commands: break, check and going to a screen", "[command]") {
    CHECK(parse_command("break")->kind == Command::Kind::Break);
    CHECK(parse_command("break 5m")->kind == Command::Kind::Break);

    const auto check = parse_command(R"(check   C:\Program Files\Steam\steam.exe  )");
    REQUIRE(check);
    CHECK(check->kind == Command::Kind::Check);
    CHECK(check->subject == R"(C:\Program Files\Steam\steam.exe)");
    CHECK(parse_command("test https://www.youtube.com/shorts/abc")->subject == "https://www.youtube.com/shorts/abc");

    CHECK(parse_command("rules")->screen == "rules");
    CHECK(parse_command("lock")->screen == "lock");
    CHECK(parse_command("unlock")->screen == "now");
    CHECK(parse_command("Status")->screen == "now");
}

TEST_CASE("Mistakes say what's wrong, and whether the command was understood", "[command]") {
    const auto unknown = parse_command("launch rockets");
    REQUIRE_FALSE(unknown);
    CHECK_FALSE(unknown.error().known_verb);
    CHECK_FALSE(parse_command("   ").error().known_verb);

    const auto mistakes = {"block", "nudge 25m", "block reddit.com 25m 2h", "block reddit.com --for",
                           "block not_a_site!", "block reddit.com --unlock wait", "lock youtube 2h --unlock maybe",
                           "lock youtube 2h --nudge", "nudge youtube --lock 2h", "block reddit.com --colour red",
                           "break now", "check"};
    for (const char* text : mistakes) {
        INFO(text);
        const auto result = parse_command(text);
        REQUIRE_FALSE(result);
        CHECK(result.error().known_verb);
        CHECK_FALSE(result.error().message.empty());
    }
}
