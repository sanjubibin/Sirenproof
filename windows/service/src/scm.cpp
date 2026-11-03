#include "scm.hpp"

#include <cstdio>
#include <cwctype>
#include <format>

#include "host.hpp"
#include "sirenproof/core/json_codec.hpp"
#include "sirenproof/platform/files.hpp"
#include "sirenproof/platform/native_host.hpp"
#include "sirenproof/platform/paths.hpp"

namespace sirenproof::service {

namespace {

namespace fs = std::filesystem;

SERVICE_STATUS_HANDLE g_status_handle = nullptr;
HANDLE g_stop = nullptr;
bool g_enforce = true;

class ScHandle {
public:
    explicit ScHandle(SC_HANDLE h) : h_(h) {}
    ~ScHandle() {
        if (h_) CloseServiceHandle(h_);
    }
    ScHandle(const ScHandle&) = delete;
    ScHandle& operator=(const ScHandle&) = delete;
    SC_HANDLE get() const { return h_; }
    explicit operator bool() const { return h_ != nullptr; }

private:
    SC_HANDLE h_;
};

void report(DWORD state, DWORD accepted, DWORD exit_code = NO_ERROR, DWORD wait_hint = 0) {
    static DWORD checkpoint = 1;
    SERVICE_STATUS status{};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = accepted;
    status.dwWin32ExitCode = exit_code == NO_ERROR ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR;
    status.dwServiceSpecificExitCode = exit_code;
    status.dwCheckPoint = (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0 : checkpoint++;
    status.dwWaitHint = wait_hint;
    SetServiceStatus(g_status_handle, &status);
}

// Windows only sends STOP while we accept it, which is never while a lock holds.
DWORD WINAPI control_handler(DWORD control, DWORD, LPVOID, LPVOID) {
    switch (control) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            SetEvent(g_stop);
            return NO_ERROR;
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI service_main(DWORD, LPWSTR*) {
    g_status_handle = RegisterServiceCtrlHandlerExW(kServiceName, control_handler, nullptr);
    if (!g_status_handle) return;
    report(SERVICE_START_PENDING, 0, NO_ERROR, 10'000);
    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    // Check the data folder before anything (even the log) is written into it.
    const fs::path data = platform::program_data_dir();
    if (!platform::ensure_trusted_directory(data, platform::kSystemAndAdminsOnly)) {
        report(SERVICE_STOPPED, 0, 1);
        return;
    }
    platform::Logger log(data / L"sirenproof.log", false);

    HostOptions options;
    options.data_dir = data;
    options.hosts_file = platform::system_hosts_file();
    options.pipe_name = std::wstring(platform::kServicePipeName);
    options.dry_run = !g_enforce;
    options.secure_data_dir = true;
    options.debug_limits = kDebugBuild;

    Host host(options, log);
    if (auto started = host.start(); !started) {
        log.error(started.error());
        report(SERVICE_STOPPED, 0, 2);
        return;
    }
    report(SERVICE_RUNNING, SERVICE_ACCEPT_SHUTDOWN);
    host.run(g_stop, [](bool stoppable) {
        report(SERVICE_RUNNING, SERVICE_ACCEPT_SHUTDOWN | (stoppable ? SERVICE_ACCEPT_STOP : 0u));
    });
    report(SERVICE_STOP_PENDING, 0, NO_ERROR, 10'000);
    host.finish();
    log.info("stopped");
    report(SERVICE_STOPPED, 0);
}

std::wstring lower(std::wstring text) {
    for (wchar_t& c : text) c = static_cast<wchar_t>(std::towlower(c));
    return text;
}

bool in_program_files(const fs::path& exe) {
    const std::wstring path = lower(exe.native());
    for (const fs::path& folder : platform::program_files_dirs()) {
        std::wstring prefix = lower(folder.native());
        if (!prefix.ends_with(L'\\')) prefix += L'\\';
        if (path.starts_with(prefix)) return true;
    }
    return false;
}

}  // namespace

int run_as_service(bool enforce) {
    g_enforce = enforce;
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<wchar_t*>(kServiceName), service_main}, {nullptr, nullptr}};
    if (!StartServiceCtrlDispatcherW(table)) {
        std::fprintf(stderr, "sirenproof-service: 'run' is for Windows to start the service. To try it by hand, use "
                             "'sirenproof-service console'.\n");
        return 1;
    }
    return 0;
}

std::expected<void, std::string> install_service(bool enforce, bool allow_any_location) {
    const fs::path exe = platform::executable_path();
    if (!allow_any_location && !in_program_files(exe)) {
        return std::unexpected(
            "install from a folder only administrators can change, such as C:\\Program Files\\Sirenproof: a service "
            "running as SYSTEM from a folder anyone can write to could be swapped for something else. "
            "(--allow-any-location skips this check; use it only in a test virtual machine.)");
    }

    const fs::path relay = exe.parent_path() / L"sirenproof-host.exe";
    if (!fs::exists(relay)) return std::unexpected("sirenproof-host.exe must be next to sirenproof-service.exe");

    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
    if (!scm) return std::unexpected("open an administrator command prompt first: " + platform::last_error_text());

    // Let Chrome, Edge and Brave start the relay for the extension.
    if (auto registered = platform::register_native_host(HKEY_LOCAL_MACHINE, relay); !registered) {
        return std::unexpected(registered.error());
    }

    const std::wstring command = L"\"" + exe.native() + L"\" run" + (enforce ? L" --enforce" : L" --dry-run");
    ScHandle service(CreateServiceW(scm.get(), kServiceName, L"Sirenproof", SERVICE_ALL_ACCESS,
                                    SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                                    command.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr));
    if (!service) {
        if (GetLastError() == ERROR_SERVICE_EXISTS) return std::unexpected("Sirenproof is already installed");
        return std::unexpected("can't create the service: " + platform::last_error_text());
    }

    std::wstring description = L"Blocks the websites and apps you chose, on your own rules. Runs only on this PC.";
    SERVICE_DESCRIPTIONW about{description.data()};
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DESCRIPTION, &about);

    // If it's killed or crashes, Windows starts it again.
    SC_ACTION actions[3] = {{SC_ACTION_RESTART, 1000}, {SC_ACTION_RESTART, 1000}, {SC_ACTION_RESTART, 5000}};
    SERVICE_FAILURE_ACTIONSW failure{86'400, nullptr, nullptr, 3, actions};
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_FAILURE_ACTIONS, &failure);
    SERVICE_FAILURE_ACTIONS_FLAG on_any_failure{TRUE};
    ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &on_any_failure);

    if (!StartServiceW(service.get(), 0, nullptr)) {
        return std::unexpected("installed, but it didn't start: " + platform::last_error_text());
    }
    return {};
}

StopCheck check_stop() {
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) return StopCheck::Locked;  // can't tell, so assume the safe answer
    ScHandle service(OpenServiceW(scm.get(), kServiceName, SERVICE_QUERY_STATUS));
    if (!service) return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST ? StopCheck::NotInstalled : StopCheck::Locked;
    SERVICE_STATUS status{};
    if (!QueryServiceStatus(service.get(), &status)) return StopCheck::Locked;
    if (status.dwCurrentState == SERVICE_STOPPED) return StopCheck::NotRunning;
    // Starting or stopping: it can't take Stop right now, so say no; asking again a moment later works.
    if (status.dwCurrentState != SERVICE_RUNNING) return StopCheck::Locked;
    return (status.dwControlsAccepted & SERVICE_ACCEPT_STOP) ? StopCheck::Stoppable : StopCheck::Locked;
}

std::expected<void, ServiceError> uninstall_service(bool remove_data) {
    const auto locked = [] {
        return std::unexpected(ServiceError{kExitLocked, "a lock is active; Sirenproof can be removed once it ends"});
    };

    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) return std::unexpected(ServiceError{1, "open an administrator command prompt first: " + platform::last_error_text()});
    ScHandle service(OpenServiceW(scm.get(), kServiceName, SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE));
    if (!service) {
        if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) return std::unexpected(ServiceError{kExitNotInstalled, "Sirenproof isn't installed"});
        return std::unexpected(ServiceError{1, "can't open the service: " + platform::last_error_text()});
    }

    // Only the running service knows whether a lock holds, so a stopped one (killed a moment ago, say) is
    // started and asked. If it can't start at all, it isn't enforcing anything, and removing it is fine:
    // a broken install must never be impossible to remove.
    SERVICE_STATUS status{};
    const auto state = [&] { return QueryServiceStatus(service.get(), &status) ? status.dwCurrentState : SERVICE_STOPPED; };
    if (state() != SERVICE_RUNNING) {
        if (!StartServiceW(service.get(), 0, nullptr) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
            std::fprintf(stderr, "sirenproof-service: the service won't start (%s), so nothing is enforced; removing it\n",
                         platform::last_error_text().c_str());
        }
        for (int i = 0; i < 150 && state() != SERVICE_RUNNING && status.dwCurrentState != SERVICE_STOPPED; ++i) Sleep(100);
    }

    // Ask it, and only believe the real one (pipe_call checks that it's the service's own process).
    if (state() == SERVICE_RUNNING) {
        const auto reply = platform::pipe_call(platform::kServicePipeName, R"({"v":1,"op":"service.can_stop"})",
                                               {std::chrono::seconds{5}, platform::executable_path()});
        if (!reply) return std::unexpected(ServiceError{1, "can't ask the service whether a lock holds: " + reply.error()});
        const core::Json answer = core::Json::parse(*reply, nullptr, false);
        if (answer.is_discarded() || !answer.value("ok", false)) {
            return std::unexpected(ServiceError{1, "the service's answer about locks was unreadable"});
        }
        if (!answer["result"].value("can_stop", false)) return locked();
    }

    // It said no lock holds. Windows only takes Stop once the service says so too, which can lag by up to
    // a second after it starts.
    for (int attempt = 0;; ++attempt) {
        if (ControlService(service.get(), SERVICE_CONTROL_STOP, &status)) break;
        const DWORD error = GetLastError();
        if (error == ERROR_SERVICE_NOT_ACTIVE) break;
        if (error == ERROR_INVALID_SERVICE_CONTROL || error == ERROR_SERVICE_CANNOT_ACCEPT_CTRL) {
            if (attempt < 30) {
                Sleep(100);
                continue;
            }
            return locked();
        }
        return std::unexpected(ServiceError{1, "can't stop the service: " + platform::error_text(error)});
    }
    for (int i = 0; i < 200 && QueryServiceStatus(service.get(), &status) && status.dwCurrentState != SERVICE_STOPPED; ++i) {
        Sleep(100);
    }

    platform::Logger log({}, true);
    platform::HostsFile hosts(platform::system_hosts_file(), false, log);
    if (auto cleaned = hosts.enforce({}); !cleaned) log.warn("couldn't clean the hosts file: " + cleaned.error());
    platform::BrowserPolicies policies(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies",
                                       platform::program_data_dir() / L"policies.json", false, log);
    if (auto restored = policies.set(false); !restored) log.warn("couldn't restore browser policies: " + restored.error());
    platform::unregister_native_host(HKEY_LOCAL_MACHINE);

    if (!DeleteService(service.get())) return std::unexpected(ServiceError{1, "can't remove the service: " + platform::last_error_text()});
    if (remove_data) {
        std::error_code ignored;
        fs::remove_all(platform::program_data_dir(), ignored);
    }
    return {};
}

}  // namespace sirenproof::service
