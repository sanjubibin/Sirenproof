#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>
#include <string_view>

#include "sirenproof/core/domain.hpp"

using sirenproof::core::Domain;
using namespace std::string_view_literals;

namespace {

Domain domain(std::string_view text) {
    auto parsed = Domain::parse(text);
    REQUIRE(parsed.has_value());
    return *parsed;
}

}  // namespace

TEST_CASE("Domain::parse normalises case and a trailing dot", "[domain]") {
    CHECK(domain("Reddit.COM").str() == "reddit.com");
    CHECK(domain("old.reddit.com.").str() == "old.reddit.com");
    CHECK(domain("WWW.YouTube.com").str() == "www.youtube.com");
}

TEST_CASE("Domain::parse accepts valid host names", "[domain]") {
    const auto text = GENERATE(as<std::string>{},
        "reddit.com",
        "m.youtube.com",
        "a.b",
        "9gag.com",
        "news.ycombinator.com",
        "my-site.co.uk",
        "xn--bcher-kva.de",    // bücher.de in punycode
        "123.example.com");    // digit-only labels are fine except the last one
    CAPTURE(text);
    CHECK(Domain::parse(text).has_value());
}

TEST_CASE("Domain::parse rejects anything that isn't a plain host name", "[domain]") {
    const auto text = GENERATE(as<std::string>{},
        "",
        ".",
        "com",                    // a whole top-level domain
        "localhost",              // single label
        "reddit..com",
        ".reddit.com",
        "reddit.com..",
        "-reddit.com",
        "reddit-.com",
        "under_score.com",
        "spa ce.com",
        " reddit.com",
        "reddit.com ",
        "tab\t.com",
        "new\nline.com",
        "evil.com\n1.2.3.4 bank.com",  // hosts-file injection attempt
        "evil.com\r\n",
        "192.168.0.1",            // IPv4 address
        "reddit.com:443",
        "https://reddit.com",
        "reddit.com/r/all",
        "*.reddit.com",
        "b\xC3\xBC" "cher.de");   // raw UTF-8 must be converted to punycode first
    CAPTURE(text);
    CHECK_FALSE(Domain::parse(text).has_value());
}

TEST_CASE("Domain::parse rejects embedded NUL characters", "[domain]") {
    CHECK_FALSE(Domain::parse("reddit\0.com"sv).has_value());
    CHECK_FALSE(Domain::parse("reddit.com\0"sv).has_value());
}

TEST_CASE("Domain::parse enforces label and name length limits", "[domain]") {
    const std::string label63(63, 'a');
    CHECK(Domain::parse(label63 + ".com").has_value());
    CHECK_FALSE(Domain::parse(label63 + "a.com").has_value());

    // 63 + 1 + 63 + 1 + 63 + 1 + 61 = 253 characters, the maximum.
    const std::string name253 = label63 + "." + label63 + "." + label63 + "." + std::string(61, 'b');
    REQUIRE(name253.size() == 253);
    CHECK(Domain::parse(name253).has_value());
    CHECK(Domain::parse(name253 + ".").has_value());  // the trailing dot doesn't count
    CHECK_FALSE(Domain::parse(name253 + "b").has_value());
}

TEST_CASE("Domain::covers matches the domain and its subdomains only", "[domain]") {
    const Domain reddit = domain("reddit.com");

    CHECK(reddit.covers(domain("reddit.com")));
    CHECK(reddit.covers(domain("old.reddit.com")));
    CHECK(reddit.covers(domain("a.b.c.reddit.com")));

    CHECK_FALSE(reddit.covers(domain("notreddit.com")));         // lookalike, no dot boundary
    CHECK_FALSE(reddit.covers(domain("reddit.com.evil.net")));   // suffix in the wrong place
    CHECK_FALSE(reddit.covers(domain("reddit.co")));
    CHECK_FALSE(domain("old.reddit.com").covers(reddit));        // a subdomain doesn't cover its parent
}

TEST_CASE("Domains compare by their normalised name", "[domain]") {
    CHECK(domain("Reddit.com") == domain("reddit.com."));
    CHECK(domain("a.com") < domain("b.com"));
}
