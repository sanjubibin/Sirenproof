#include <catch2/catch_test_macros.hpp>

#include "sirenproof/core/hosts.hpp"

using namespace sirenproof::core;

namespace {

std::vector<Domain> sites(std::initializer_list<std::string_view> names) {
    std::vector<Domain> out;
    for (std::string_view n : names) out.push_back(*Domain::parse(n));
    return out;
}

const std::string kUserHosts =
    "# Copyright (c) 1993-2009 Microsoft Corp.\r\n"
    "#\r\n"
    "127.0.0.1 my-dev-site.test\r\n";

}  // namespace

TEST_CASE("hosts_names_for adds the common prefixes once", "[hosts]") {
    CHECK(hosts_names_for(*Domain::parse("reddit.com")) ==
          std::vector<std::string>{"reddit.com", "www.reddit.com", "m.reddit.com"});
    CHECK(hosts_names_for(*Domain::parse("www.example.com")) ==
          std::vector<std::string>{"www.example.com", "m.www.example.com"});
}

TEST_CASE("The section is appended with the file's line endings, IPv4 and IPv6", "[hosts]") {
    const std::string out = apply_hosts_section(kUserHosts, sites({"reddit.com"}));
    CHECK(out == kUserHosts +
                     std::string(kHostsBegin) + "\r\n"
                     "0.0.0.0 m.reddit.com\r\n:: m.reddit.com\r\n"
                     "0.0.0.0 reddit.com\r\n:: reddit.com\r\n"
                     "0.0.0.0 www.reddit.com\r\n:: www.reddit.com\r\n" +
                     std::string(kHostsEnd) + "\r\n");
}

TEST_CASE("Updating replaces the old section and leaves everything else alone", "[hosts]") {
    const std::string first = apply_hosts_section(kUserHosts, sites({"reddit.com", "x.com"}));
    const std::string second = apply_hosts_section(first, sites({"x.com"}));
    CHECK(second == apply_hosts_section(kUserHosts, sites({"x.com"})));
    CHECK(second.find("reddit") == std::string::npos);
    CHECK(second.starts_with(kUserHosts));
}

TEST_CASE("An empty list removes the section completely", "[hosts]") {
    const std::string with = apply_hosts_section(kUserHosts, sites({"reddit.com"}));
    CHECK(apply_hosts_section(with, {}) == kUserHosts);
    CHECK(apply_hosts_section(kUserHosts, {}) == kUserHosts);  // nothing to remove: unchanged
}

TEST_CASE("Applying the same list twice changes nothing", "[hosts]") {
    const std::string once = apply_hosts_section(kUserHosts, sites({"reddit.com"}));
    CHECK(apply_hosts_section(once, sites({"reddit.com"})) == once);
}

TEST_CASE("User lines after the section and LF-only files are kept", "[hosts]") {
    const std::string lf = "127.0.0.1 a.test\n";
    const std::string with = apply_hosts_section(lf, sites({"x.com"}));
    CHECK(with.find("\r\n") == std::string::npos);
    const std::string user_after = with + "127.0.0.1 b.test\n";
    CHECK(apply_hosts_section(user_after, {}) == "127.0.0.1 a.test\n127.0.0.1 b.test\n");
}

TEST_CASE("A file without a final newline gets one before the section", "[hosts]") {
    const std::string out = apply_hosts_section("127.0.0.1 a.test", sites({"x.com"}));
    CHECK(out.starts_with("127.0.0.1 a.test\r\n"));
    CHECK(apply_hosts_section(out, {}) == "127.0.0.1 a.test\r\n");
}

TEST_CASE("A damaged section loses only its own lines", "[hosts]") {
    // Someone deleted the END line: the BEGIN line and our entries go; the user's line after stays.
    const std::string damaged = std::string("127.0.0.1 a.test\r\n") + std::string(kHostsBegin) +
                                "\r\n0.0.0.0 x.com\r\n:: x.com\r\n127.0.0.1 b.test\r\n";
    CHECK(apply_hosts_section(damaged, {}) == "127.0.0.1 a.test\r\n127.0.0.1 b.test\r\n");
}

TEST_CASE("Duplicate sections are all replaced by one", "[hosts]") {
    const std::string section = apply_hosts_section("", sites({"x.com"}));
    const std::string doubled = kUserHosts + section + section;
    CHECK(apply_hosts_section(doubled, sites({"x.com"})) == kUserHosts + section);
}
