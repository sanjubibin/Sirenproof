#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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
// - A program a Nudge rule catches is neither closed nor frozen: it's marked paused, and the tray covers
//   the screen with the pause. It stays paused until the rule lets it go on (the person chose to
//   continue) or the person asks for it to be closed (close_paused).
// - With dry_run nothing is closed; it only logs what it would do.
class AppBlocker {
public:
    using Decide = std::function<core::Verdict(const core::AppIdentity&)>;
    using Clock = std::chrono::system_clock;

    // A program waiting at the pause.
    struct Paused {
        std::string id;  // "n1", "n2"...: the same for as long as it waits
        std::uint32_t pid = 0;
        std::string path;
        std::string rule_id;
        Clock::time_point since;
    };

    AppBlocker(bool dry_run, Logger& log, std::chrono::seconds grace = std::chrono::seconds{10});

    // One pass over the running programs. `active_rules` are the rules in force (from Plan), used to
    // tell when a block began. Returns how many programs were closed.
    int sweep(const Decide& decide, const std::vector<std::string>& active_rules, Clock::time_point now);

    // Forget everything, e.g. when no program rules are active any more.
    void reset();

    std::vector<Paused> paused() const;

    // "Close it" at the pause: closes that one program, and only while it's still waiting there. In a
    // dry run it's only logged, and the program stops waiting.
    std::expected<void, std::string> close_paused(std::string_view id);

private:
    struct Tracked {
        std::wstring exe_name;
        ProcessDetails details;
        bool eligible = true;  // false for processes that must never be closed
        Clock::time_point inspected{};
        std::optional<Clock::time_point> close_at;
        bool reported = false;
        std::string pause_id;  // set while it waits at the pause
        std::string pause_rule;
        Clock::time_point paused_since{};
        bool pause_dismissed = false;  // dry run: "closed" at the pause, so not paused again
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
    std::uint64_t pauses_ = 0;
};

}  // namespace sirenproof::platform
