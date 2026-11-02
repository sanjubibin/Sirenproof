#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "sirenproof/core/policy.hpp"
#include "sirenproof/platform/log.hpp"
#include "sirenproof/platform/process.hpp"

namespace sirenproof::platform {

// Closes programs that a rule blocks.
//
// - A program started while its block was already on is closed straight away: there's no work in it
//   to lose. One that was already running when the block began gets `grace` first, so it can be saved.
// - Never touches Sirenproof itself, Windows services (session 0), critical processes or anything in
//   the Windows folder, whatever a rule says.
// - Nudges need the pause screen from the tray app, so for now they're only logged.
// - With dry_run nothing is closed; it only logs what it would do.
class AppBlocker {
public:
    using Decide = std::function<core::Verdict(const core::AppIdentity&)>;
    using Clock = std::chrono::system_clock;

    AppBlocker(bool dry_run, Logger& log, std::chrono::seconds grace = std::chrono::seconds{10});

    // One pass over the running programs. `active_rules` are the rules in force (from Plan), used to
    // tell when a block began. Returns how many programs were closed.
    int sweep(const Decide& decide, const std::vector<std::string>& active_rules, Clock::time_point now);

    // Forget everything, e.g. when no program rules are active any more.
    void reset();

private:
    struct Tracked {
        std::wstring exe_name;
        ProcessDetails details;
        bool eligible = true;  // false for processes that must never be closed
        Clock::time_point inspected{};
        std::optional<Clock::time_point> close_at;
        bool reported = false;
    };

    bool eligible(const ProcessDetails& details) const;

    bool dry_run_;
    Logger& log_;
    std::chrono::seconds grace_;
    std::uint32_t self_pid_;
    std::wstring windows_dir_;  // lower case, ends with '\'
    AppIdentityCache identities_;
    std::map<std::uint32_t, Tracked> processes_;
    std::map<std::string, Clock::time_point> rule_since_;
};

}  // namespace sirenproof::platform
