#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "sirenproof/core/app.hpp"
#include "sirenproof/core/domain.hpp"
#include "sirenproof/core/url.hpp"

namespace sirenproof::core {

// Anything a rule can block or allow.
using Target = std::variant<Domain, PagePattern, AppPattern>;

// Text form, used in the state file, the CLI and the command palette:
//
//   reddit.com                         a whole site (with subdomains)
//   youtube.com/shorts                 part of a site (see PagePattern)
//   app:discord.exe                    a program by name
//   app:C:\Games\Steam\game.exe        a program by path
//   app:C:\Games\Steam\                every program in a folder (note the trailing '\')
//   publisher:Valve Corp.              every program signed by a publisher
//   package:<family name>              a Microsoft Store app
//
// Web targets take no scheme: "reddit.com", not "https://reddit.com".
std::optional<Target> parse_target(std::string_view text);
std::string to_string(const Target& target);

bool is_web_target(const Target& target);

// Safety rail: why this target may never be blocked, or nullopt if it's fine. Covers Windows itself,
// Windows Update and sign-in, security software, assistive technology and Sirenproof.
std::optional<std::string> protected_reason(const Target& target);

}  // namespace sirenproof::core
