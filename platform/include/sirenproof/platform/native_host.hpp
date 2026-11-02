#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "sirenproof/platform/win.hpp"

namespace sirenproof::platform {

inline constexpr std::wstring_view kNativeHostName = L"com.sirenproof.host";

// Tells Chrome, Edge and Brave where Sirenproof's native messaging host is: writes the host manifest
// (sirenproof-host.json) next to `host_exe` and points each browser's registry key at it. `root` is
// HKEY_LOCAL_MACHINE for an installed Sirenproof, or HKEY_CURRENT_USER for development.
std::expected<void, std::string> register_native_host(HKEY root, const std::filesystem::path& host_exe);
void unregister_native_host(HKEY root);

}  // namespace sirenproof::platform
