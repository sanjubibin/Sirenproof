#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "sirenproof/core/url.hpp"

using namespace sirenproof::core;

namespace {

Url url(std::string_view text) {
    auto u = Url::parse(text);
    REQUIRE(u.has_value());
    return *u;
}

PagePattern page(std::string_view text) {
    auto p = PagePattern::parse(text);
    REQUIRE(p.has_value());
    return *p;
}

}  // namespace

TEST_CASE("Url::parse extracts host, path and query", "[url]") {
    const Url u = url("https://www.YouTube.com/Shorts/abc?x=1#comments");
    CHECK(u.host.str() == "www.youtube.com");
    CHECK(u.path == "/shorts/abc");
    CHECK(u.query == "x=1");

    CHECK(url("youtube.com").path == "/");
    const Url with_port = url("http://user@reddit.com:8080/r/all");
    CHECK(with_port.host.str() == "reddit.com");
    CHECK(with_port.path == "/r/all");
}

TEST_CASE("Url::parse rejects non-web addresses", "[url]") {
    const auto text = GENERATE(as<std::string>{}, "chrome://settings", "about:blank", "file:///c:/x.html",
                               "ftp://example.com/", "http://192.168.0.1/", "http://[::1]/", "", "https://");
    CAPTURE(text);
    CHECK_FALSE(Url::parse(text).has_value());
}

TEST_CASE("normalise_path removes trivial variations", "[url]") {
    CHECK(normalise_path("") == "/");
    CHECK(normalise_path("/Shorts") == "/shorts");
    CHECK(normalise_path("/%53horts") == "/shorts");          // %53 is 'S'
    CHECK(normalise_path("//shorts///abc") == "/shorts/abc");
    CHECK(normalise_path("/%2Fx") == "/%2fx");                // an encoded '/' stays encoded
    CHECK(normalise_path("/%zz") == "/%zz");
}

TEST_CASE("PagePattern::parse accepts host-and-path forms only", "[url]") {
    CHECK(page("youtube.com/shorts").path == "/shorts");
    CHECK(page("youtube.com/shorts/").path == "/shorts");
    CHECK(page("youtube.com/").path == "/");
    CHECK(page("youtube.com/watch?v=abc").query.size() == 1);

    const auto text = GENERATE(as<std::string>{}, "youtube.com", "https://youtube.com/x", "youtube.com/a b",
                               "youtube.com/x#y", "youtube.com/?", "youtube.com/?=1", "not a host/x");
    CAPTURE(text);
    CHECK_FALSE(PagePattern::parse(text).has_value());
}

TEST_CASE("PagePattern matches whole path segments, case-insensitively", "[url]") {
    const PagePattern shorts = page("youtube.com/shorts");
    CHECK(shorts.matches(url("https://m.youtube.com/shorts/abc")));
    CHECK(shorts.matches(url("youtube.com/shorts")));
    CHECK(shorts.matches(url("youtube.com/SHORTS/x")));
    CHECK(shorts.matches(url("youtube.com//shorts")));
    CHECK(shorts.matches(url("youtube.com/%73horts/x")));

    CHECK_FALSE(shorts.matches(url("youtube.com/shortsfoo")));
    CHECK_FALSE(shorts.matches(url("youtube.com/feed")));
    CHECK_FALSE(shorts.matches(url("notyoutube.com/shorts")));
}

TEST_CASE("PagePattern root path means the home page only", "[url]") {
    const PagePattern home = page("youtube.com/");
    CHECK(home.matches(url("youtube.com")));
    CHECK(home.matches(url("https://www.youtube.com/?app=desktop")));
    CHECK_FALSE(home.matches(url("youtube.com/watch?v=1")));
}

TEST_CASE("PagePattern query parameters must all be present", "[url]") {
    const PagePattern video = page("youtube.com/watch?v=abc");
    CHECK(video.matches(url("youtube.com/watch?v=abc&t=10")));
    CHECK(video.matches(url("youtube.com/watch?t=10&v=abc")));
    CHECK_FALSE(video.matches(url("youtube.com/watch?v=ABC")));  // video IDs are case-sensitive
    CHECK_FALSE(video.matches(url("youtube.com/watch?list=1")));

    const PagePattern any_video = page("youtube.com/watch?v");
    CHECK(any_video.matches(url("youtube.com/watch?v=anything")));
    CHECK_FALSE(any_video.matches(url("youtube.com/watch")));
}

TEST_CASE("PagePattern::to_string round-trips", "[url]") {
    for (const char* text : {"youtube.com/shorts", "youtube.com/", "youtube.com/watch?v=abc&t", "m.reddit.com/r/all"}) {
        CHECK(page(text).to_string() == text);
        CHECK(page(page(text).to_string()) == page(text));
    }
}
