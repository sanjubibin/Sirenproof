#pragma once

#include <chrono>
#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "sirenproof/platform/log.hpp"
#include "sirenproof/platform/win.hpp"

namespace sirenproof::platform {

// Browser policies Sirenproof sets while a lock that blocks websites holds:
// - private windows off (Incognito, InPrivate), since the extension doesn't run in them;
// - DNS-over-HTTPS off, so the hosts file applies.
//
// Every value is recorded in a ledger file before it's changed, so turning the policies off puts back
// exactly what was there, including values set by someone else. Browsers show "Managed by your
// organization" only while they're on.
class BrowserPolicies {
public:
    // `root` and `policies_path` are normally HKEY_LOCAL_MACHINE and "SOFTWARE\Policies"; tests and
    // console mode point them somewhere harmless.
    BrowserPolicies(HKEY root, std::wstring policies_path, std::filesystem::path ledger, bool dry_run, Logger& log);

    // Idempotent; only writes a value when it differs from what it should be.
    std::expected<void, std::string> set(bool on);

private:
    struct Wanted {
        std::wstring key;
        std::wstring name;
        DWORD type;
        std::vector<std::uint8_t> data;
    };
    std::vector<Wanted> wanted() const;

    HKEY root_;
    std::wstring policies_path_;
    std::filesystem::path ledger_;
    bool dry_run_;
    Logger& log_;
    std::optional<bool> dry_run_state_;
};

// While a lock that blocks websites holds, every open browser must have Sirenproof's extension
// connected (shown by its heartbeats). A browser without it (or one the extension doesn't support
// yet, such as Firefox) gets a warning in the log, then `grace`, then its windows are closed.
// A freshly started browser gets a few seconds for the extension to connect.
class BrowserGuard {
public:
    using Clock = std::chrono::steady_clock;

    BrowserGuard(bool dry_run, Logger& log, std::chrono::seconds grace = std::chrono::seconds{60},
                 std::chrono::seconds startup_window = std::chrono::seconds{15});

    void heartbeat(const std::filesystem::path& browser, Clock::time_point now);

    // One pass while a web lock holds. Returns how many processes were closed.
    int enforce(Clock::time_point now);

    // No web lock: forget countdowns.
    void relax() { close_at_.clear(); }

    // Browsers seen without the extension, for the status report.
    std::vector<std::string> unprotected() const;

    // Tests: treat these exe names as browsers (with extension support) instead of the real list.
    void set_browser_names_for_testing(std::vector<std::wstring> names) { test_names_ = std::move(names); }

    static constexpr std::chrono::seconds kHeartbeatTimeout{30};

private:
    bool is_browser(const std::wstring& exe_name, bool& extension_available) const;

    bool dry_run_;
    Logger& log_;
    std::chrono::seconds grace_;
    std::chrono::seconds startup_window_;  // a new browser gets this long for the extension to connect
    std::map<std::wstring, Clock::time_point> heartbeats_;  // lower-case exe path -> last heartbeat
    std::map<std::wstring, Clock::time_point> close_at_;    // lower-case exe path -> when to close
    std::map<std::wstring, std::string> display_names_;
    std::vector<std::wstring> test_names_;
};

}  // namespace sirenproof::platform
