// sirenproof-service: the background service that keeps the rules.
//
//   run         what Windows starts (after install)
//   console     run in this window, for development
//   install     register and start the service (administrator)
//   uninstall   stop and remove it, and clean the hosts file (administrator)

#include <cstdio>
#include <string>
#include <vector>

#include "host.hpp"
#include "scm.hpp"
#include "sirenproof/core/version.hpp"
#include "sirenproof/platform/files.hpp"
#include "sirenproof/platform/paths.hpp"

using namespace sirenproof;

namespace {

namespace fs = std::filesystem;

// Release builds enforce by default. Debug builds only log what they would do unless told otherwise.
constexpr bool kEnforceByDefault = !service::kDebugBuild;

constexpr const char* kUsage = R"(Usage:
  sirenproof-service console [--data-dir <dir>] [--hosts-file <file>] [--pipe <name>] [--enforce | --dry-run]
                             [--real-browser-policies] [--guard-browsers]
  sirenproof-service install [--enforce | --dry-run] [--allow-any-location]
  sirenproof-service uninstall [--remove-data]
  sirenproof-service --version

console   Runs the service in this window until Ctrl+C. Defaults are safe for development:
          data in .\dev-data next to this exe, and a stand-in hosts file there too. The real hosts
          file is only touched if you pass it with --hosts-file, and real browser policies only with
          --real-browser-policies. Browsers without the extension are never closed in console mode
          unless you pass --guard-browsers.
install   Needs an administrator prompt, and this exe in a folder like C:\Program Files\Sirenproof.
          --allow-any-location skips that check: only for a test virtual machine.
)";

HANDLE g_console_stop = nullptr;

BOOL WINAPI on_console_signal(DWORD) {
    SetEvent(g_console_stop);
    return TRUE;
}

bool take_flag(std::vector<std::wstring>& args, const wchar_t* flag) {
    for (auto it = args.begin(); it != args.end(); ++it) {
        if (*it == flag) {
            args.erase(it);
            return true;
        }
    }
    return false;
}

std::wstring take_value(std::vector<std::wstring>& args, const wchar_t* flag) {
    for (auto it = args.begin(); it != args.end(); ++it) {
        if (*it == flag && it + 1 != args.end()) {
            std::wstring value = *(it + 1);
            args.erase(it, it + 2);
            return value;
        }
    }
    return {};
}

bool enforce_option(std::vector<std::wstring>& args) {
    if (take_flag(args, L"--enforce")) return true;
    if (take_flag(args, L"--dry-run")) return false;
    return kEnforceByDefault;
}

int fail(const std::string& message) {
    std::fprintf(stderr, "sirenproof-service: %s\n", message.c_str());
    return 1;
}

int run_console(std::vector<std::wstring> args) {
    service::HostOptions options;
    const std::wstring data_dir = take_value(args, L"--data-dir");
    const std::wstring hosts_file = take_value(args, L"--hosts-file");
    const std::wstring pipe = take_value(args, L"--pipe");
    const bool real_policies = take_flag(args, L"--real-browser-policies");
    options.guard_browsers = take_flag(args, L"--guard-browsers");
    options.dry_run = !enforce_option(args);
    if (!args.empty()) return fail("unknown option " + platform::narrow(args.front()));

    // Like the stand-in hosts file: browser policies go somewhere browsers don't read, unless asked.
    if (real_policies) {
        options.policies_root = HKEY_LOCAL_MACHINE;
        options.policies_path = L"SOFTWARE\\Policies";
    } else {
        options.policies_root = HKEY_CURRENT_USER;
        options.policies_path = L"Software\\SirenproofDev\\StandInPolicies";
    }

    options.data_dir = data_dir.empty() ? platform::executable_dir() / L"dev-data" : fs::path(data_dir);
    options.hosts_file = hosts_file.empty() ? options.data_dir / L"hosts" : fs::path(hosts_file);
    options.pipe_name = pipe.empty() ? std::wstring(platform::kServicePipeName) : pipe;
    options.secure_data_dir = false;
    options.debug_limits = service::kDebugBuild;

    if (auto ok = platform::ensure_trusted_directory(options.data_dir, std::nullopt); !ok) return fail(ok.error());
    platform::Logger log(options.data_dir / L"sirenproof.log", true);
    g_console_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    SetConsoleCtrlHandler(on_console_signal, TRUE);

    service::Host host(options, log);
    if (auto started = host.start(); !started) return fail(started.error());
    log.info("console mode: press Ctrl+C to stop");
    host.run(g_console_stop, [&](bool stoppable) {
        log.info(stoppable ? "no lock holds: the installed service could be stopped now"
                           : "a lock holds: the installed service would refuse to stop");
    });
    host.finish();
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::wstring> args(argv + 1, argv + argc);
    if (args.empty() || args[0] == L"--help" || args[0] == L"-h" || args[0] == L"help") {
        std::fputs(kUsage, stdout);
        return args.empty() ? 1 : 0;
    }
    if (args[0] == L"--version") {
        std::printf("sirenproof-service %s%s\n", std::string(kVersion).c_str(), service::kDebugBuild ? " (debug build)" : "");
        return 0;
    }

    const std::wstring command = args[0];
    args.erase(args.begin());

    if (command == L"run") return service::run_as_service(enforce_option(args));
    if (command == L"console") return run_console(std::move(args));
    if (command == L"install") {
        const bool enforce = enforce_option(args);
        const bool anywhere = take_flag(args, L"--allow-any-location");
        if (auto ok = service::install_service(enforce, anywhere); !ok) return fail(ok.error());
        std::printf("Sirenproof is installed and running%s.\n", enforce ? "" : " in dry-run mode (nothing is enforced)");
        return 0;
    }
    if (command == L"uninstall") {
        if (auto ok = service::uninstall_service(take_flag(args, L"--remove-data")); !ok) return fail(ok.error());
        std::printf("Sirenproof is uninstalled and the hosts file is clean.\n");
        return 0;
    }
    std::fputs(kUsage, stderr);
    return 1;
}
