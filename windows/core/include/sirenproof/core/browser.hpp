#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace sirenproof::core {

// --- Native messaging ------------------------------------------------------------------------------
// Chrome-family browsers talk to a native "host" program over stdin/stdout: each message is a 4-byte
// little-endian length followed by that many bytes of UTF-8 JSON.

// Our own limit for messages from the browser (Chrome would allow up to 4 GB).
inline constexpr std::size_t kMaxFromBrowser = 64 * 1024;
// Chrome's limit for messages to the browser.
inline constexpr std::size_t kMaxToBrowser = 1024 * 1024;

std::array<std::uint8_t, 4> encode_length(std::uint32_t length);
std::uint32_t decode_length(std::span<const std::uint8_t, 4> bytes);
std::string frame_message(std::string_view json);

// The extension's id, from the public key in extension/manifest.json. A store listing gets its own id,
// which joins this list when the extension is published.
inline constexpr std::array<std::string_view, 1> kExtensionIds = {"cgpgilkcgaojdaclgkllhndfgdphgdje"};

// True if `origin` (as Chrome passes it to the host, "chrome-extension://<id>/") is our extension.
bool is_our_extension(std::string_view origin);

// The only operations the browser relay may ask the service for.
inline constexpr std::array<std::string_view, 4> kExtensionOps = {"ext.hello", "ext.heartbeat", "ext.decide",
                                                                  "ext.continue"};
bool is_extension_op(std::string_view op);

// --- Browsers --------------------------------------------------------------------------------------

struct BrowserInfo {
    std::string_view name;
    bool extension_available;  // can run Sirenproof's extension (Chrome family) in this version
};

// Known browsers by exe file name (case-insensitive), e.g. "chrome.exe" -> {"Chrome", true}.
std::optional<BrowserInfo> browser_for_exe(std::string_view exe_name);

}  // namespace sirenproof::core
