#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "sirenproof/core/app.hpp"

using namespace sirenproof::core;
using Kind = AppPattern::Kind;

namespace {

AppPattern program(std::string_view spec) {
    auto p = AppPattern::parse_program(spec);
    REQUIRE(p.has_value());
    return *p;
}

AppIdentity at_path(std::string path) { return AppIdentity{std::move(path), "", "", ""}; }

}  // namespace

TEST_CASE("normalise_app_path lower-cases and unifies separators", "[app]") {
    CHECK(normalise_app_path("\\\\?\\C:/Games//Steam\\Steam.exe") == "c:\\games\\steam\\steam.exe");
}

TEST_CASE("AppPattern::parse_program tells names, paths and folders apart", "[app]") {
    CHECK(program("Discord") == AppPattern{Kind::Name, "discord.exe"});
    CHECK(program("steam.exe") == AppPattern{Kind::Name, "steam.exe"});
    CHECK(program("C:\\Games\\Steam\\") == AppPattern{Kind::Folder, "c:\\games\\steam\\"});
    CHECK(program("C:/Games/x.exe") == AppPattern{Kind::Path, "c:\\games\\x.exe"});
}

TEST_CASE("AppPattern rejects malformed programs", "[app]") {
    const auto spec = GENERATE(as<std::string>{}, "", "   ", "a<b.exe", "C:\\..\\x.exe", "C:\\a\\..\\b\\",
                               "\\\\server\\share\\x.exe", "relative\\path.exe", "..", "x\ny.exe");
    CAPTURE(spec);
    CHECK_FALSE(AppPattern::parse_program(spec).has_value());
}

TEST_CASE("A name pattern also matches a renamed copy by its original filename", "[app]") {
    const AppPattern discord = program("discord.exe");
    CHECK(discord.matches(at_path("C:\\Users\\me\\AppData\\Local\\Discord\\app-1.0\\Discord.exe")));

    AppIdentity renamed = at_path("C:\\Users\\me\\Desktop\\notes.exe");
    CHECK_FALSE(discord.matches(renamed));
    renamed.original_filename = "Discord.exe";
    CHECK(discord.matches(renamed));
}

TEST_CASE("A folder pattern matches everything inside it, and only that", "[app]") {
    const AppPattern steam = program("C:\\Games\\Steam\\");
    CHECK(steam.matches(at_path("C:\\Games\\Steam\\steamapps\\common\\Game\\game.exe")));
    CHECK(steam.matches(at_path("c:/games/steam/steam.exe")));
    CHECK_FALSE(steam.matches(at_path("C:\\Games\\SteamTools\\tool.exe")));
}

TEST_CASE("Publisher and package patterns compare case-insensitively", "[app]") {
    const auto valve = AppPattern::parse(Kind::Publisher, " Valve Corp. ");
    REQUIRE(valve);
    CHECK(valve->value == "valve corp.");
    CHECK(valve->matches(AppIdentity{"C:\\x\\game.exe", "", "Valve Corp.", ""}));
    CHECK_FALSE(valve->matches(AppIdentity{"C:\\x\\game.exe", "", "", ""}));

    const auto minecraft = AppPattern::parse(Kind::Package, "Microsoft.MinecraftUWP_8wekyb3d8bbwe");
    REQUIRE(minecraft);
    CHECK(minecraft->matches(AppIdentity{"", "", "", "microsoft.minecraftuwp_8wekyb3d8bbwe"}));
    CHECK_FALSE(AppPattern::parse(Kind::Package, "bad name").has_value());
}
