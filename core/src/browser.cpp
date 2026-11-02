#include "sirenproof/core/browser.hpp"

#include <algorithm>

namespace sirenproof::core {

namespace {

struct KnownBrowser {
    std::string_view exe;
    BrowserInfo info;
};

constexpr std::array<KnownBrowser, 7> kBrowsers = {{
    {"chrome.exe", {"Chrome", true}},
    {"msedge.exe", {"Edge", true}},
    {"brave.exe", {"Brave", true}},
    {"vivaldi.exe", {"Vivaldi", true}},
    {"opera.exe", {"Opera", false}},       // Chromium-based, but its own extension store and policies
    {"firefox.exe", {"Firefox", false}},   // planned for v1.x
    {"librewolf.exe", {"LibreWolf", false}},
}};

std::string lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    return out;
}

}  // namespace

std::array<std::uint8_t, 4> encode_length(std::uint32_t length) {
    return {static_cast<std::uint8_t>(length), static_cast<std::uint8_t>(length >> 8),
            static_cast<std::uint8_t>(length >> 16), static_cast<std::uint8_t>(length >> 24)};
}

std::uint32_t decode_length(std::span<const std::uint8_t, 4> bytes) {
    return std::uint32_t{bytes[0]} | (std::uint32_t{bytes[1]} << 8) | (std::uint32_t{bytes[2]} << 16) |
           (std::uint32_t{bytes[3]} << 24);
}

std::string frame_message(std::string_view json) {
    const auto header = encode_length(static_cast<std::uint32_t>(json.size()));
    std::string framed(header.begin(), header.end());
    framed.append(json);
    return framed;
}

bool is_our_extension(std::string_view origin) {
    constexpr std::string_view prefix = "chrome-extension://";
    if (!origin.starts_with(prefix)) return false;
    origin.remove_prefix(prefix.size());
    if (origin.ends_with('/')) origin.remove_suffix(1);
    return std::ranges::find(kExtensionIds, origin) != kExtensionIds.end();
}

bool is_extension_op(std::string_view op) { return std::ranges::find(kExtensionOps, op) != kExtensionOps.end(); }

std::optional<BrowserInfo> browser_for_exe(std::string_view exe_name) {
    const std::string name = lower(exe_name);
    for (const KnownBrowser& b : kBrowsers) {
        if (b.exe == name) return b.info;
    }
    return std::nullopt;
}

}  // namespace sirenproof::core
