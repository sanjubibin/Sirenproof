#include <catch2/catch_test_macros.hpp>

#include <string>

#include "sirenproof/core/version.hpp"

TEST_CASE("The version string matches its numeric parts", "[version]") {
    const std::string expected = std::to_string(sirenproof::kVersionMajor) + "." +
                                 std::to_string(sirenproof::kVersionMinor) + "." +
                                 std::to_string(sirenproof::kVersionPatch);
    CHECK(sirenproof::kVersion == expected);
}
