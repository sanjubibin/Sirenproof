#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "sirenproof/core/clock.hpp"
#include "sirenproof/core/policy.hpp"
#include "sirenproof/platform/app_blocker.hpp"
#include "sirenproof/platform/browsers.hpp"
#include "sirenproof/platform/hosts_file.hpp"
#include "sirenproof/platform/log.hpp"
#include "sirenproof/platform/pipe.hpp"
#include "sirenproof/platform/process.hpp"
#include "sirenproof/platform/state_store.hpp"
#include "sirenproof/platform/system.hpp"

namespace sirenproof::service {

struct HostOptions {
    std::filesystem::path data_dir;
    std::filesystem::path hosts_file;
    std::wstring pipe_name;
    bool dry_run = true;
    bool secure_data_dir = true;  // the installed service: SYSTEM/Administrators-only data folder
    bool debug_limits = false;    // debug builds: lock sessions capped at 2 minutes, dev-unlock file honoured
    // Where browser policies go: the real place for the installed service, a stand-in for console mode.
    HKEY policies_root = HKEY_LOCAL_MACHINE;
    std::wstring policies_path = L"SOFTWARE\\Policies";
    // Close browsers without the extension during web locks. Console mode only logs it unless asked.
    bool guard_browsers = true;
};

// The running service: owns the state, keeps trusted time, enforces the plan and answers the pipe.
class Host {
public:
    Host(HostOptions options, platform::Logger& log);
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    // Loads the saved state and opens the pipe.
    std::expected<void, std::string> start();

    // Runs until `stop` is signalled. `on_stoppable` hears whenever stopping becomes allowed or not:
    // never while a lock holds.
    void run(HANDLE stop, const std::function<void(bool)>& on_stoppable);

    // Saves the state one last time and closes the pipe.
    void finish();

    // Answers one pipe request (see protocol.cpp).
    std::string handle(std::string_view request, const platform::PipeClient& client);

    bool can_stop();

    static constexpr std::chrono::minutes kDebugLockCap{2};

private:
    void step();                     // one pass; the caller holds mutex_
    void save(bool force);           // the caller holds mutex_
    bool can_stop_locked(core::TimePoint now) const;
    core::TimePoint now() const { return clock_->now(); }
    void warn_once(std::string& last, const std::string& message);

    HostOptions options_;
    platform::Logger& log_;
    std::mutex mutex_;
    core::State state_;
    std::optional<core::TrustedClock> clock_;
    core::Engine engine_;
    platform::StateStore store_;
    platform::HostsFile hosts_;
    platform::AppBlocker apps_;
    platform::BrowserPolicies policies_;
    platform::BrowserGuard browsers_;
    bool web_lock_ = false;
    platform::AppIdentityCache identities_;
    platform::SystemRandom random_;
    std::optional<platform::PipeServer> pipe_;
    platform::Handle wake_;

    std::string last_saved_;  // state without the clock, as last written
    std::chrono::steady_clock::time_point last_save_{};
    std::string last_hosts_error_;
    std::string last_policy_error_;
    std::string last_clock_warning_;
    std::string last_zone_warning_;
};

}  // namespace sirenproof::service
