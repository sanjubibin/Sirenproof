#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace sirenproof::core {

// What the platform knows about a running program.
struct AppIdentity {
    std::string path;               // full image path
    std::string original_filename;  // from the version resource; empty if unknown
    std::string publisher;          // Authenticode signer; empty if unsigned or unknown
    std::string package_family;     // Store apps only
};

// Path form used for matching: lower-case ASCII, '/' turned into '\', repeated '\' collapsed and a
// leading "\\?\" removed. Non-ASCII letters keep their case; the platform canonicalises paths before
// they get here.
std::string normalise_app_path(std::string_view path);

// A rule target for desktop programs.
//
//   Name       "discord.exe"            matches the file name *or* the original filename, so renaming
//                                       or copying the exe doesn't get around the rule
//   Path       "c:\games\x\game.exe"    that exact file
//   Folder     "c:\games\steam\"        everything inside, at any depth
//   Publisher  "valve corp."            everything signed by that publisher
//   Package    "<family name>"          a Microsoft Store app
struct AppPattern {
    enum class Kind { Name, Path, Folder, Publisher, Package };

    Kind kind;
    std::string value;  // normalised as described above

    // `spec` is what follows the target prefix: "discord.exe", "C:\Games\Steam\" and so on.
    // A name without an extension gets ".exe".
    static std::optional<AppPattern> parse(Kind kind, std::string_view spec);
    // Name, Path or Folder, decided by the shape of `spec`.
    static std::optional<AppPattern> parse_program(std::string_view spec);

    bool matches(const AppIdentity& app) const;

    friend bool operator==(const AppPattern&, const AppPattern&) = default;
};

}  // namespace sirenproof::core
