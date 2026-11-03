#include <catch2/catch_test_macros.hpp>

#include "sirenproof/core/browser.hpp"

using namespace sirenproof::core;

TEST_CASE("Native messaging lengths are 4 bytes, little-endian", "[browser]") {
    CHECK(encode_length(0x01020304u) == std::array<std::uint8_t, 4>{0x04, 0x03, 0x02, 0x01});
    const std::array<std::uint8_t, 4> bytes{0x10, 0x00, 0x00, 0x00};
    CHECK(decode_length(bytes) == 16);
    CHECK(decode_length(encode_length(65'536)) == 65'536);

    const std::string framed = frame_message(R"({"op":"x"})");
    REQUIRE(framed.size() == 4 + 10);
    CHECK(framed[0] == 10);
    CHECK(framed.substr(4) == R"({"op":"x"})");
}

TEST_CASE("Only our extension's origin is accepted", "[browser][security]") {
    CHECK(is_our_extension("chrome-extension://cgpgilkcgaojdaclgkllhndfgdphgdje/"));
    CHECK(is_our_extension("chrome-extension://cgpgilkcgaojdaclgkllhndfgdphgdje"));
    CHECK_FALSE(is_our_extension("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/"));
    CHECK_FALSE(is_our_extension("https://cgpgilkcgaojdaclgkllhndfgdphgdje/"));
    CHECK_FALSE(is_our_extension(""));
}

TEST_CASE("The relay may only ask for extension operations", "[browser][security]") {
    CHECK(is_extension_op("ext.decide"));
    CHECK(is_extension_op("ext.heartbeat"));
    CHECK_FALSE(is_extension_op("rules.remove"));
    CHECK_FALSE(is_extension_op("unlock.request"));
    CHECK_FALSE(is_extension_op("ext."));
}

TEST_CASE("Known browsers are recognised by exe name", "[browser]") {
    REQUIRE(browser_for_exe("Chrome.exe"));
    CHECK(browser_for_exe("chrome.exe")->name == "Chrome");
    CHECK(browser_for_exe("MSEDGE.EXE")->extension_available);
    REQUIRE(browser_for_exe("firefox.exe"));
    CHECK_FALSE(browser_for_exe("firefox.exe")->extension_available);
    CHECK_FALSE(browser_for_exe("notepad.exe"));
}
