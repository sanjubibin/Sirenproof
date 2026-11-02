#include <catch2/catch_test_macros.hpp>

#include <string>

#include "sirenproof/core/crypto.hpp"
#include "test_support.hpp"

using namespace sirenproof::core;

TEST_CASE("sha256 matches the FIPS 180-4 test vectors", "[crypto]") {
    CHECK(to_hex(sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(to_hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(to_hex(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(to_hex(sha256("The quick brown fox jumps over the lazy dog")) ==
          "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
    CHECK(to_hex(sha256(std::string(1'000'000, 'a'))) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("hmac_sha256 matches the RFC 4231 test vectors", "[crypto]") {
    const std::vector<std::uint8_t> key1(20, 0x0b);
    CHECK(to_hex(hmac_sha256(key1, "Hi There")) ==
          "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

    const std::string jefe = "Jefe";
    const std::vector<std::uint8_t> key2(jefe.begin(), jefe.end());
    CHECK(to_hex(hmac_sha256(key2, "what do ya want for nothing?")) ==
          "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

    // Test case 6: a key longer than the block size is hashed first.
    const std::vector<std::uint8_t> key6(131, 0xaa);
    CHECK(to_hex(hmac_sha256(key6, "Test Using Larger Than Block-Size Key - Hash Key First")) ==
          "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
}

TEST_CASE("hex encoding round-trips and rejects junk", "[crypto]") {
    const std::vector<std::uint8_t> bytes = {0x00, 0x7f, 0x80, 0xff};
    CHECK(to_hex(bytes) == "007f80ff");
    CHECK(from_hex("007F80ff") == bytes);
    CHECK_FALSE(from_hex("abc"));
    CHECK_FALSE(from_hex("zz"));
}

TEST_CASE("constant_time_equal compares contents and length", "[crypto]") {
    const std::vector<std::uint8_t> a = {1, 2, 3};
    const std::vector<std::uint8_t> b = {1, 2, 3};
    const std::vector<std::uint8_t> c = {1, 2, 4};
    const std::vector<std::uint8_t> d = {1, 2};
    CHECK(constant_time_equal(a, b));
    CHECK_FALSE(constant_time_equal(a, c));
    CHECK_FALSE(constant_time_equal(a, d));
}

TEST_CASE("random_code uses the unambiguous alphabet", "[crypto]") {
    sirenproof::test::CountingRandom random;
    CHECK(random_code(random, 5, false) == "01234");

    const std::string grouped = random_code(random, 15, true);
    REQUIRE(grouped.size() == 17);
    CHECK(grouped[5] == '-');
    CHECK(grouped[11] == '-');
    for (const char c : grouped) {
        CHECK(std::string_view("0123456789abcdefghjkmnpqrstvwxyz-").find(c) != std::string_view::npos);
    }
}

TEST_CASE("normalise_code forgives case, separators and look-alike letters", "[crypto]") {
    CHECK(normalise_code("ABCDE-FGHJK o i l") == "abcdefghjk011");
    CHECK(normalise_code("x7k2m") == "x7k2m");
    CHECK_FALSE(normalise_code("abcu"));  // 'u' isn't in the alphabet
    CHECK_FALSE(normalise_code("ab!c"));
}
