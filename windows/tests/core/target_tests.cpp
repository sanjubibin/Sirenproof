#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "sirenproof/core/target.hpp"
#include "test_support.hpp"

using namespace sirenproof::core;
using sirenproof::test::target;

TEST_CASE("parse_target recognises every target form and round-trips it", "[target]") {
    const auto text = GENERATE(as<std::string>{}, "reddit.com", "youtube.com/shorts", "youtube.com/watch?v=abc",
                               "app:discord.exe", "app:c:\\games\\steam\\", "app:c:\\games\\x\\game.exe",
                               "publisher:valve corp.", "package:microsoft.minecraftuwp_8wekyb3d8bbwe");
    CAPTURE(text);
    const Target t = target(text);
    CHECK(to_string(t) == text);
    CHECK(parse_target(to_string(t)) == t);
}

TEST_CASE("parse_target rejects malformed targets", "[target]") {
    const auto text = GENERATE(as<std::string>{}, "", "https://reddit.com", "app:", "publisher:", "package:bad name",
                               "reddit", "app:C:\\Windows\\..\\x.exe");
    CAPTURE(text);
    CHECK_FALSE(parse_target(text).has_value());
}

TEST_CASE("Web and program targets are told apart", "[target]") {
    CHECK(is_web_target(target("reddit.com")));
    CHECK(is_web_target(target("youtube.com/shorts")));
    CHECK_FALSE(is_web_target(target("app:steam.exe")));
}

TEST_CASE("Protected targets are refused, with a reason", "[target][safety]") {
    const auto text = GENERATE(as<std::string>{},
                               "microsoft.com",              // would cover update.microsoft.com
                               "login.live.com",
                               "foo.windowsupdate.com",
                               "login.live.com/oauth",       // a page on a sign-in domain
                               "app:explorer.exe",
                               "app:svchost.exe",
                               "app:narrator.exe",           // assistive technology
                               "app:nvda.exe",
                               "app:msmpeng.exe",            // security
                               "app:sirenproof-service.exe", // ourselves
                               "app:C:\\Windows\\notepad.exe",
                               "app:C:\\Windows\\System32\\",
                               "app:C:\\",                   // would cover C:\Windows
                               "publisher:Microsoft Corporation");
    CAPTURE(text);
    CHECK(protected_reason(target(text)).has_value());
}

TEST_CASE("Ordinary targets are not protected", "[target][safety]") {
    const auto text = GENERATE(as<std::string>{}, "reddit.com", "youtube.com/shorts", "microsoft.com/xbox",
                               "app:steam.exe", "app:C:\\Games\\", "publisher:Valve Corp.",
                               "package:microsoft.minecraftuwp_8wekyb3d8bbwe");
    CAPTURE(text);
    CHECK_FALSE(protected_reason(target(text)).has_value());
}
