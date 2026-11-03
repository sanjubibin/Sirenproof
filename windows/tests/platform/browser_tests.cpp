// Browser-related Windows tests: policies on a throwaway registry key, the guard with the sleeper
// standing in for a browser, and the relay as a child process. Real browsers and real policies are
// never touched.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <optional>
#include <string>

#include "sirenproof/core/browser.hpp"
#include "sirenproof/core/json_codec.hpp"
#include "sirenproof/platform/browsers.hpp"
#include "sirenproof/platform/pipe.hpp"
#include "sirenproof/platform/process.hpp"
#include "support.hpp"

using namespace sirenproof;
using namespace sirenproof::platform;
using namespace sirenproof::test;

namespace {

struct TestRegistryKey {
    std::wstring path;
    TestRegistryKey() {
        static std::atomic<int> counter{0};
        path = std::format(L"Software\\SirenproofTest\\{}-{}", GetCurrentProcessId(), ++counter);
    }
    ~TestRegistryKey() {
        RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str());
        RegDeleteKeyW(HKEY_CURRENT_USER, path.c_str());
        RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\SirenproofTest");  // only succeeds once empty
    }
};

std::optional<std::wstring> reg_string(const std::wstring& key, const wchar_t* name) {
    wchar_t buffer[256] = {};
    DWORD size = sizeof(buffer);
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return std::wstring(buffer);
}

std::optional<DWORD> reg_dword(const std::wstring& key, const wchar_t* name) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return value;
}

void set_reg(const std::wstring& key, const wchar_t* name, DWORD type, const void* data, DWORD size) {
    HKEY handle = nullptr;
    REQUIRE(RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr) ==
            ERROR_SUCCESS);
    RegSetValueExW(handle, name, 0, type, static_cast<const BYTE*>(data), size);
    RegCloseKey(handle);
}

// sirenproof-host.exe as a child process, with its stdin and stdout connected to the test.
struct RelayProcess {
    PROCESS_INFORMATION info{};
    HANDLE to_child = nullptr;
    HANDLE from_child = nullptr;

    explicit RelayProcess(const std::wstring& arguments) {
        SECURITY_ATTRIBUTES inheritable{sizeof(inheritable), nullptr, TRUE};
        HANDLE child_in = nullptr;
        HANDLE child_out = nullptr;
        REQUIRE(CreatePipe(&child_in, &to_child, &inheritable, 0));
        REQUIRE(CreatePipe(&from_child, &child_out, &inheritable, 0));
        SetHandleInformation(to_child, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(from_child, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = child_in;
        startup.hStdOutput = child_out;
        startup.hStdError = child_out;
        std::wstring command = L"\"" + widen(SIRENPROOF_HOST_EXE) + L"\" " + arguments;
        const BOOL started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                            nullptr, &startup, &info);
        CloseHandle(child_in);
        CloseHandle(child_out);
        REQUIRE(started);
    }
    ~RelayProcess() {
        CloseHandle(to_child);
        CloseHandle(from_child);
        TerminateProcess(info.hProcess, 0);
        CloseHandle(info.hThread);
        CloseHandle(info.hProcess);
    }
    RelayProcess(const RelayProcess&) = delete;
    RelayProcess& operator=(const RelayProcess&) = delete;

    void send_raw(std::string_view bytes) {
        DWORD written = 0;
        WriteFile(to_child, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    }
    void send(std::string_view json) { send_raw(core::frame_message(json)); }

    std::string receive() {
        std::array<std::uint8_t, 4> header{};
        DWORD read = 0;
        REQUIRE(ReadFile(from_child, header.data(), 4, &read, nullptr));
        REQUIRE(read == 4);
        std::string body(core::decode_length(header), '\0');
        std::size_t total = 0;
        while (total < body.size()) {
            REQUIRE(ReadFile(from_child, body.data() + total, static_cast<DWORD>(body.size() - total), &read, nullptr));
            total += read;
        }
        return body;
    }

    std::optional<DWORD> exit_code_within(DWORD ms) const {
        if (WaitForSingleObject(info.hProcess, ms) != WAIT_OBJECT_0) return std::nullopt;
        DWORD code = 0;
        GetExitCodeProcess(info.hProcess, &code);
        return code;
    }
};

}  // namespace

// ---------------------------------------------------------------------------------------------------
// Policies

TEST_CASE("BrowserPolicies turns private windows and DNS-over-HTTPS off, then restores what was there",
          "[platform][browsers]") {
    TempDir dir;
    TestRegistryKey reg;
    Logger log = quiet_log();
    const std::wstring chrome = reg.path + L"\\Policies\\Google\\Chrome";
    const std::wstring edge = reg.path + L"\\Policies\\Microsoft\\Edge";

    // Someone had already chosen a DNS-over-HTTPS mode for Chrome.
    const std::wstring automatic = L"automatic";
    set_reg(chrome, L"DnsOverHttpsMode", REG_SZ, automatic.c_str(),
            static_cast<DWORD>((automatic.size() + 1) * sizeof(wchar_t)));

    BrowserPolicies policies(HKEY_CURRENT_USER, reg.path + L"\\Policies", dir.path / L"policies.json", false, log);
    REQUIRE(policies.set(true));
    CHECK(reg_dword(chrome, L"IncognitoModeAvailability") == 1u);
    CHECK(reg_string(chrome, L"DnsOverHttpsMode") == L"off");
    CHECK(reg_dword(edge, L"InPrivateModeAvailability") == 1u);
    CHECK(reg_string(edge, L"DnsOverHttpsMode") == L"off");

    // Incognito switched back on during the lock: the next pass turns it off again.
    const DWORD enabled = 0;
    set_reg(chrome, L"IncognitoModeAvailability", REG_DWORD, &enabled, sizeof(enabled));
    REQUIRE(policies.set(true));
    CHECK(reg_dword(chrome, L"IncognitoModeAvailability") == 1u);

    // The lock is over: exactly what was there before comes back.
    REQUIRE(policies.set(false));
    CHECK(reg_string(chrome, L"DnsOverHttpsMode") == L"automatic");
    CHECK_FALSE(reg_dword(chrome, L"IncognitoModeAvailability"));
    CHECK_FALSE(reg_dword(edge, L"InPrivateModeAvailability"));
    CHECK_FALSE(fs::exists(dir.path / L"policies.json"));
}

TEST_CASE("BrowserPolicies in dry-run mode writes nothing", "[platform][browsers]") {
    TempDir dir;
    TestRegistryKey reg;
    Logger log = quiet_log();
    BrowserPolicies policies(HKEY_CURRENT_USER, reg.path + L"\\Policies", dir.path / L"policies.json", true, log);
    REQUIRE(policies.set(true));
    CHECK_FALSE(reg_dword(reg.path + L"\\Policies\\Google\\Chrome", L"IncognitoModeAvailability"));
}

// ---------------------------------------------------------------------------------------------------
// Guard

TEST_CASE("BrowserGuard spares a browser with the extension and closes one without it", "[platform][browsers]") {
    if (!in_desktop_session()) SKIP("needs a desktop session: Sirenproof never closes session-0 services");
    Logger log = quiet_log();
    Sleeper sleeper;  // stands in for a browser
    const auto details = inspect_process(sleeper.info.dwProcessId);
    REQUIRE(details);

    BrowserGuard guard(false, log, std::chrono::seconds{2}, std::chrono::seconds{0});
    guard.set_browser_names_for_testing({L"sleeper.exe"});
    const auto t0 = BrowserGuard::Clock::now();

    guard.heartbeat(widen(details->identity.path), t0);
    CHECK(guard.enforce(t0 + std::chrono::seconds{3}) == 0);  // the extension is connected
    CHECK(guard.unprotected().empty());

    // 40 s without a heartbeat: a warning, then the grace period, then closed.
    CHECK(guard.enforce(t0 + std::chrono::seconds{40}) == 0);
    CHECK(guard.unprotected().size() == 1);
    CHECK_FALSE(sleeper.exited_within(100));
    CHECK(guard.enforce(t0 + std::chrono::seconds{43}) == 1);
    CHECK(sleeper.exited_within(5000));
}

TEST_CASE("browser_ancestor finds no browser for a program something else started", "[platform][browsers]") {
    Sleeper sleeper;
    CHECK_FALSE(browser_ancestor(sleeper.info.dwProcessId).has_value());
}

// ---------------------------------------------------------------------------------------------------
// Relay

TEST_CASE("The relay passes extension requests on and answers with the same id", "[platform][relay]") {
    Logger log = quiet_log();
    const std::wstring name = unique_pipe_name();
    PipeServer server(name, [](std::string_view request, const PipeClient&) {
        const auto j = core::Json::parse(request);
        return core::Json{{"ok", true}, {"result", {{"op", j["op"]}, {"url", j["args"].value("url", "")}}}}.dump();
    }, log);
    REQUIRE(server.start());

    RelayProcess relay(L"relay --pipe " + name + L" --trust-any-server");
    relay.send(R"({"id":5,"op":"ext.decide","args":{"url":"https://www.reddit.com/"}})");
    const auto reply = core::Json::parse(relay.receive());
    CHECK(reply["id"] == 5);
    CHECK(reply["ok"] == true);
    CHECK(reply["result"]["op"] == "ext.decide");
    CHECK(reply["result"]["url"] == "https://www.reddit.com/");
}

TEST_CASE("The relay refuses anything but extension operations, without asking the service",
          "[platform][relay][security]") {
    Logger log = quiet_log();
    const std::wstring name = unique_pipe_name();
    std::atomic<int> asked{0};
    PipeServer server(name, [&](std::string_view, const PipeClient&) {
        ++asked;
        return std::string(R"({"ok":true,"result":{}})");
    }, log);
    REQUIRE(server.start());

    RelayProcess relay(L"relay --pipe " + name + L" --trust-any-server");
    relay.send(R"({"id":6,"op":"rules.remove","args":{"id":"work"}})");
    const auto refused = core::Json::parse(relay.receive());
    CHECK(refused["id"] == 6);
    CHECK(refused["ok"] == false);
    CHECK(refused["error"].get<std::string>().find("may not ask") != std::string::npos);

    relay.send("not json");
    CHECK(core::Json::parse(relay.receive())["ok"] == false);
    CHECK(asked == 0);
}

TEST_CASE("The relay only believes the real service", "[platform][relay][security]") {
    Logger log = quiet_log();
    const std::wstring name = unique_pipe_name();
    PipeServer impostor(name, [](std::string_view, const PipeClient&) {
        return std::string(R"({"ok":true,"result":{"action":"allow"}})");
    }, log);
    REQUIRE(impostor.start());

    RelayProcess relay(L"relay --pipe " + name);  // no --trust-any-server: must be sirenproof-service.exe
    relay.send(R"({"id":1,"op":"ext.decide","args":{"url":"https://www.reddit.com/"}})");
    const auto reply = core::Json::parse(relay.receive());
    CHECK(reply["ok"] == false);
    CHECK(reply["error"].get<std::string>().find("something other than") != std::string::npos);
}

TEST_CASE("The relay stops on an oversized message and ignores other extensions", "[platform][relay][security]") {
    RelayProcess flooding(L"relay --pipe \\\\.\\pipe\\Sirenproof.nobody --trust-any-server");
    const auto too_long = core::encode_length(static_cast<std::uint32_t>(core::kMaxFromBrowser + 1));
    flooding.send_raw(std::string_view(reinterpret_cast<const char*>(too_long.data()), too_long.size()));
    CHECK(flooding.exit_code_within(5000) == 1u);

    RelayProcess stranger(L"chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/");
    CHECK(stranger.exit_code_within(5000) == 2u);
}
