#pragma once

#include <expected>
#include <string>
#include <vector>

namespace sirenproof::platform {

// Who can administer this PC, for the "Lock down this PC" setup. Read only: Sirenproof never changes
// accounts; the person does that in Windows' own settings, where they can see it.
struct Administrators {
    std::wstring current_user;         // the account this program runs as, e.g. "sanju"
    bool current_is_admin = false;     // whether that account is in the local Administrators group
    std::vector<std::wstring> others;  // the group's other accounts, leaving out disabled ones
};

// Reads the local Administrators group as it is now, so a change made in Settings shows at once,
// before anyone signs out. Only accounts listed in the group directly count; a domain group inside it
// can't be looked into from here.
std::expected<Administrators, std::string> read_administrators();

}  // namespace sirenproof::platform
