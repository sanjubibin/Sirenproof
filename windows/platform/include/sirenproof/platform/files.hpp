#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace sirenproof::platform {

// Only SYSTEM and Administrators, inherited by everything inside, and not inheriting from the parent.
inline constexpr std::wstring_view kSystemAndAdminsOnly = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";

std::expected<std::string, std::string> read_file(const std::filesystem::path& path);

// Writes through a temporary file in the same folder, then swaps it in with one call, so nobody ever
// reads half a file. An existing file keeps its permissions and attributes (read-only included).
std::expected<void, std::string> write_file_atomic(const std::filesystem::path& path, std::string_view content);

// Makes sure `dir` exists and can be trusted before a privileged process writes into it:
// - it must not be a junction or symbolic link (that could redirect writes somewhere else);
// - if `sddl` is given, an existing folder must be owned by SYSTEM or Administrators, since ordinary
//   users can create folders under ProgramData before Sirenproof is installed; its permissions are
//   then reset to `sddl`.
std::expected<void, std::string> ensure_trusted_directory(const std::filesystem::path& dir,
                                                          std::optional<std::wstring_view> sddl);

}  // namespace sirenproof::platform
