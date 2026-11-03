#include "sirenproof/core/crypto.hpp"

#include <algorithm>
#include <bit>

namespace sirenproof::core {

namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

constexpr std::string_view kCodeAlphabet = "0123456789abcdefghjkmnpqrstvwxyz";
static_assert(kCodeAlphabet.size() == 32);

void compress(std::array<std::uint32_t, 8>& state, const std::uint8_t* block) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = (std::uint32_t{block[4 * i]} << 24) | (std::uint32_t{block[4 * i + 1]} << 16) |
               (std::uint32_t{block[4 * i + 2]} << 8) | std::uint32_t{block[4 * i + 3]};
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    auto [a, b, c, d, e, f, g, h] = state;
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const std::uint32_t choice = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + s1 + choice + kRoundConstants[i] + w[i];
        const std::uint32_t s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

}  // namespace

Sha256Digest sha256(std::span<const std::uint8_t> data) {
    std::array<std::uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    std::size_t offset = 0;
    for (; offset + 64 <= data.size(); offset += 64) compress(state, data.data() + offset);

    // Final block(s): remaining bytes, a 1 bit, zero padding, then the length in bits (big-endian).
    std::array<std::uint8_t, 128> tail{};
    const std::size_t remaining = data.size() - offset;
    for (std::size_t i = 0; i < remaining; ++i) tail[i] = data[offset + i];
    tail[remaining] = 0x80;
    const std::size_t tail_size = remaining < 56 ? 64 : 128;
    const std::uint64_t bit_length = std::uint64_t{data.size()} * 8;
    for (std::size_t i = 0; i < 8; ++i) {
        tail[tail_size - 1 - i] = static_cast<std::uint8_t>(bit_length >> (8 * i));
    }
    compress(state, tail.data());
    if (tail_size == 128) compress(state, tail.data() + 64);

    Sha256Digest digest{};
    for (std::size_t i = 0; i < 8; ++i) {
        digest[4 * i] = static_cast<std::uint8_t>(state[i] >> 24);
        digest[4 * i + 1] = static_cast<std::uint8_t>(state[i] >> 16);
        digest[4 * i + 2] = static_cast<std::uint8_t>(state[i] >> 8);
        digest[4 * i + 3] = static_cast<std::uint8_t>(state[i]);
    }
    return digest;
}

Sha256Digest sha256(std::string_view text) {
    return sha256(std::span{reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

Sha256Digest hmac_sha256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> message) {
    constexpr std::size_t kBlockSize = 64;
    std::array<std::uint8_t, kBlockSize> block{};
    if (key.size() > kBlockSize) {
        const Sha256Digest hashed = sha256(key);
        std::ranges::copy(hashed, block.begin());
    } else {
        std::ranges::copy(key, block.begin());
    }

    std::vector<std::uint8_t> inner(kBlockSize + message.size());
    for (std::size_t i = 0; i < kBlockSize; ++i) inner[i] = static_cast<std::uint8_t>(block[i] ^ 0x36u);
    std::ranges::copy(message, inner.begin() + kBlockSize);
    const Sha256Digest inner_hash = sha256(inner);

    std::array<std::uint8_t, kBlockSize + 32> outer{};
    for (std::size_t i = 0; i < kBlockSize; ++i) outer[i] = static_cast<std::uint8_t>(block[i] ^ 0x5cu);
    std::ranges::copy(inner_hash, outer.begin() + kBlockSize);
    return sha256(outer);
}

Sha256Digest hmac_sha256(std::span<const std::uint8_t> key, std::string_view message) {
    return hmac_sha256(key, std::span{reinterpret_cast<const std::uint8_t*>(message.data()), message.size()});
}

std::string to_hex(std::span<const std::uint8_t> bytes) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (const std::uint8_t b : bytes) {
        hex.push_back(digits[b >> 4]);
        hex.push_back(digits[b & 0x0f]);
    }
    return hex;
}

std::optional<std::vector<std::uint8_t>> from_hex(std::string_view hex) {
    if (hex.size() % 2 != 0) return std::nullopt;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = nibble(hex[i]);
        const int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        bytes.push_back(static_cast<std::uint8_t>(hi * 16 + lo));
    }
    return bytes;
}

bool constant_time_equal(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b) {
    if (a.size() != b.size()) return false;
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) diff = static_cast<std::uint8_t>(diff | (a[i] ^ b[i]));
    return diff == 0;
}

std::string random_code(RandomSource& random, std::size_t length, bool grouped) {
    std::vector<std::uint8_t> bytes(length);
    random.fill(bytes);
    std::string code;
    code.reserve(length + length / 5);
    for (std::size_t i = 0; i < length; ++i) {
        if (grouped && i > 0 && i % 5 == 0) code.push_back('-');
        code.push_back(kCodeAlphabet[bytes[i] & 31u]);  // 256 is a multiple of 32, so this is uniform
    }
    return code;
}

std::optional<std::string> normalise_code(std::string_view typed) {
    std::string code;
    code.reserve(typed.size());
    for (char c : typed) {
        if (c == '-' || c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c == 'o') c = '0';
        if (c == 'i' || c == 'l') c = '1';
        if (kCodeAlphabet.find(c) == std::string_view::npos) return std::nullopt;
        code.push_back(c);
    }
    return code;
}

}  // namespace sirenproof::core
