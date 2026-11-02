#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "sirenproof/core/domain.hpp"

namespace sirenproof::core {

// Sirenproof keeps all its hosts-file entries between these two lines and never touches anything else.
inline constexpr std::string_view kHostsBegin = "# BEGIN Sirenproof (managed automatically; changes here are undone)";
inline constexpr std::string_view kHostsEnd = "# END Sirenproof";

// The host names written for a blocked site. The hosts file has no wildcards, so besides the site we
// add its most common prefixes ("www.", "m."); the browser extension covers the rest.
std::vector<std::string> hosts_names_for(const Domain& site);

// Returns `existing` with Sirenproof's section replaced by entries that block `sites` (IPv4 and IPv6),
// or with the section removed if `sites` is empty.
//
// - Everything outside the section is kept exactly as it was.
// - A damaged section (a BEGIN line without an END line) is removed along with the entry lines that
//   follow it, but nothing past the first line that isn't one of ours.
// - The section uses the file's own line ending (CRLF for a new file).
// - Entries only ever point at 0.0.0.0 and ::, never at a real address.
std::string apply_hosts_section(std::string_view existing, const std::vector<Domain>& sites);

}  // namespace sirenproof::core
