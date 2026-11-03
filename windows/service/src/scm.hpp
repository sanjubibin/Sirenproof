#pragma once

#include <expected>
#include <string>

namespace sirenproof::service {

inline constexpr const wchar_t* kServiceName = L"Sirenproof";

#ifdef SIRENPROOF_DEBUG_BUILD
inline constexpr bool kDebugBuild = true;
#else
inline constexpr bool kDebugBuild = false;
#endif

// Hands this process to the Windows service control manager (the "run" command).
int run_as_service(bool enforce);

// Registers and starts the service (needs an administrator prompt). Refuses unless this exe lives in
// a folder only administrators can change, since a SYSTEM service anyone could replace would be a way
// to take over the PC; allow_any_location exists for test virtual machines.
std::expected<void, std::string> install_service(bool enforce, bool allow_any_location);

// Exit codes the installer relies on (installer/sirenproof.iss).
inline constexpr int kExitLocked = 3;        // a lock holds, so the service can't be stopped
inline constexpr int kExitNotInstalled = 4;  // there's no service to remove

struct ServiceError {
    int exit_code = 1;
    std::string message;
};

// Whether the installed service could be stopped right now, as Windows sees it: the service only
// accepts Stop while no lock holds.
enum class StopCheck { NotInstalled, NotRunning, Stoppable, Locked };
StopCheck check_stop();

// Stops and removes the service, and removes Sirenproof's section from the hosts file. Refuses while a
// lock holds.
std::expected<void, ServiceError> uninstall_service(bool remove_data);

}  // namespace sirenproof::service
