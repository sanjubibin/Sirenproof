#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sirenproof::core {

using Sha256Digest = std::array<std::uint8_t, 32>;

// FIPS 180-4 SHA-256. Used to store friend unlock codes as salted hashes, never in plain text.
Sha256Digest sha256(std::span<const std::uint8_t> data);
Sha256Digest sha256(std::string_view text);

// RFC 2104 HMAC-SHA-256. Used to notice when the saved state was edited by hand.
Sha256Digest hmac_sha256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> message);
Sha256Digest hmac_sha256(std::span<const std::uint8_t> key, std::string_view message);

std::string to_hex(std::span<const std::uint8_t> bytes);
std::optional<std::vector<std::uint8_t>> from_hex(std::string_view hex);

// Compares in time that depends only on the lengths, so a wrong guess leaks nothing through timing.
bool constant_time_equal(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b);

// Source of cryptographically secure random bytes. The platform implements it (BCryptGenRandom);
// tests use a deterministic one.
class RandomSource {
public:
    virtual ~RandomSource() = default;
    virtual void fill(std::span<std::uint8_t> out) = 0;
};

// Random text from the 32-symbol Crockford alphabet "0123456789abcdefghjkmnpqrstvwxyz" (no i, l, o, u,
// so nothing is easy to misread). With `grouped`, a '-' goes between groups of five.
std::string random_code(RandomSource& random, std::size_t length, bool grouped);

// Canonical form of a typed code: lower-case, separators, spaces and line breaks removed, and the look-alikes
// mapped back (o -> 0, i/l -> 1). Returns nullopt if anything else is outside the alphabet.
std::optional<std::string> normalise_code(std::string_view typed);

}  // namespace sirenproof::core
