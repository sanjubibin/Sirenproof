// Integration tests for the Windows layer. They use temporary files, a private pipe name and a sleeper
// process started for the purpose; nothing here touches the real hosts file, services or other programs.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <string>

#include "sirenproof/core/hosts.hpp"
#include "sirenproof/core/rule.hpp"
#include "sirenproof/core/state_json.hpp"
#include "sirenproof/core/target.hpp"
#include "sirenproof/platform/app_blocker.hpp"
#include "sirenproof/platform/files.hpp"
#include "sirenproof/platform/hosts_file.hpp"
#include "sirenproof/platform/paths.hpp"
#include "sirenproof/platform/pipe.hpp"
#include "sirenproof/platform/process.hpp"
#include "sirenproof/platform/state_store.hpp"
#include "sirenproof/platform/system.hpp"

namespace fs = std::filesystem;
using namespace sirenproof;
using namespace sirenproof::platform;

namespace {

// A fresh folder under %TEMP%, removed afterwards.
struct TempDir {
    fs::path path;
    TempDir() {
        static std::atomic<int> counter{0};
        path = fs::temp_directory_path() / std::format(L"sirenproof-test-{}-{}", GetCurrentProcessId(), ++counter);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ignored;
        for (const auto& entry : fs::recursive_directory_iterator(path, ignored)) {
            SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        fs::remove_all(path, ignored);
    }
};

std::string read(const fs::path& p) {
    auto text = read_file(p);
    REQUIRE(text.has_value());
    return *text;
}

std::wstring unique_pipe_name() {
    static std::atomic<int> counter{0};
    return std::format(L"\\\\.\\pipe\\Sirenproof.test-{}-{}", GetCurrentProcessId(), ++counter);
}

Logger quiet_log() { return Logger({}, false); }

// CI machines run tests as an elevated administrator, sometimes as a service in session 0; a few tests
// only mean something for an ordinary user at a desktop.
bool running_elevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) &&
                          elevation.TokenIsElevated != 0;
    CloseHandle(token);
    return elevated;
}

bool in_desktop_session() {
    DWORD session = 0;
    return ProcessIdToSessionId(GetCurrentProcessId(), &session) && session != 0;
}

// The sleeper test program, started suspended-free and closed again by the test or its destructor.
struct Sleeper {
    PROCESS_INFORMATION info{};
    Sleeper() {
        std::wstring command = L"\"" + widen(SIRENPROOF_SLEEPER_EXE) + L"\"";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        REQUIRE(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                               &startup, &info));
    }
    ~Sleeper() {
        TerminateProcess(info.hProcess, 0);
        CloseHandle(info.hThread);
        CloseHandle(info.hProcess);
    }
    bool exited_within(DWORD ms) const { return WaitForSingleObject(info.hProcess, ms) == WAIT_OBJECT_0; }
};

}  // namespace

// ---------------------------------------------------------------------------------------------------
// Files

TEST_CASE("write_file_atomic creates and replaces files", "[platform][files]") {
    TempDir dir;
    const fs::path file = dir.path / L"data.txt";
    REQUIRE(write_file_atomic(file, "first"));
    CHECK(read(file) == "first");
    REQUIRE(write_file_atomic(file, "second, longer"));
    CHECK(read(file) == "second, longer");
    CHECK(std::distance(fs::directory_iterator(dir.path), fs::directory_iterator{}) == 1);  // no temp files left
}

TEST_CASE("write_file_atomic keeps a read-only file read-only", "[platform][files]") {
    TempDir dir;
    const fs::path file = dir.path / L"hosts";
    REQUIRE(write_file_atomic(file, "old"));
    REQUIRE(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY));
    REQUIRE(write_file_atomic(file, "new"));
    CHECK(read(file) == "new");
    CHECK((GetFileAttributesW(file.c_str()) & FILE_ATTRIBUTE_READONLY) != 0);
}

TEST_CASE("ensure_trusted_directory creates folders and rejects non-folders", "[platform][files]") {
    TempDir dir;
    CHECK(ensure_trusted_directory(dir.path / L"a" / L"b", std::nullopt));
    CHECK(fs::is_directory(dir.path / L"a" / L"b"));
    REQUIRE(write_file_atomic(dir.path / L"file", "x"));
    CHECK_FALSE(ensure_trusted_directory(dir.path / L"file", std::nullopt));
}

TEST_CASE("ensure_trusted_directory refuses a folder an ordinary user owns", "[platform][files][security]") {
    // Run as an ordinary user, a folder this test creates is exactly what an attacker could plant under
    // ProgramData before Sirenproof is installed. As an administrator the folder is rightly trusted.
    if (running_elevated()) SKIP("running as administrator: the folder would be owned by Administrators");
    TempDir dir;
    const auto result = ensure_trusted_directory(dir.path, kSystemAndAdminsOnly);
    REQUIRE_FALSE(result);
    CHECK(result.error().find("can't be trusted") != std::string::npos);
}

// ---------------------------------------------------------------------------------------------------
// Hosts file (a temporary copy, never the real one)

TEST_CASE("HostsFile adds, repairs and removes its section, leaving the rest alone", "[platform][hosts]") {
    TempDir dir;
    const fs::path hosts = dir.path / L"hosts";
    const std::string original = "# user content\r\n127.0.0.1 my.test\r\n";
    REQUIRE(write_file_atomic(hosts, original));
    Logger log = quiet_log();
    HostsFile file(hosts, false, log);
    const std::vector<core::Domain> sites = {*core::Domain::parse("reddit.com")};

    CHECK(file.enforce(sites) == true);
    CHECK(read(hosts).find("0.0.0.0 reddit.com") != std::string::npos);
    CHECK(file.enforce(sites) == false);  // already right

    // Someone deletes the section: the next pass puts it back.
    REQUIRE(write_file_atomic(hosts, original));
    CHECK(file.enforce(sites) == true);
    CHECK(read(hosts) == core::apply_hosts_section(original, sites));

    CHECK(file.enforce({}) == true);
    CHECK(read(hosts) == original);
}

TEST_CASE("HostsFile in dry-run mode never writes", "[platform][hosts]") {
    TempDir dir;
    const fs::path hosts = dir.path / L"hosts";
    REQUIRE(write_file_atomic(hosts, "127.0.0.1 my.test\r\n"));
    Logger log = quiet_log();
    HostsFile file(hosts, true, log);
    CHECK(file.enforce({*core::Domain::parse("reddit.com")}) == true);
    CHECK(read(hosts) == "127.0.0.1 my.test\r\n");
}

// ---------------------------------------------------------------------------------------------------
// State store

TEST_CASE("StateStore saves, loads and signs the state", "[platform][state]") {
    TempDir dir;
    StateStore store(dir.path);
    auto first = store.load();
    REQUIRE(first);
    CHECK(first->fresh);

    core::State state;
    core::Rule rule;
    rule.id = "work";
    rule.block = {*core::parse_target("reddit.com")};
    state.rules = {rule};
    REQUIRE(store.save(state));

    StateStore again(dir.path);
    auto loaded = again.load();
    REQUIRE(loaded);
    CHECK_FALSE(loaded->fresh);
    CHECK(loaded->warning.empty());
    CHECK(loaded->state == state);
    CHECK(read(dir.path / L"state.dat").starts_with("SIRENPROOF-STATE 1\n"));
}

TEST_CASE("An edited state is never used; the backup is", "[platform][state][tamper]") {
    TempDir dir;
    StateStore store(dir.path);
    REQUIRE(store.load());

    core::State older;
    older.settings.breaks_per_day = 1;
    REQUIRE(store.save(older));
    core::State newer = older;
    newer.settings.breaks_per_day = 2;
    REQUIRE(store.save(newer));  // older becomes the backup

    // Edit the saved JSON by hand: allow more breaks.
    std::string text = read(dir.path / L"state.dat");
    const std::size_t at = text.find("\"breaks_per_day\": 2");
    REQUIRE(at != std::string::npos);
    text.replace(at, 19, "\"breaks_per_day\": 9");
    REQUIRE(write_file_atomic(dir.path / L"state.dat", text));

    StateStore reopened(dir.path);
    auto loaded = reopened.load();
    REQUIRE(loaded);
    CHECK(loaded->state.settings.breaks_per_day == 1);
    CHECK(loaded->warning.find("signature") != std::string::npos);

    // Both edited: refuse.
    REQUIRE(write_file_atomic(dir.path / L"state.dat.bak", text));
    StateStore broken(dir.path);
    CHECK_FALSE(broken.load());
}

TEST_CASE("A state without its key can't be trusted", "[platform][state][tamper]") {
    TempDir dir;
    StateStore store(dir.path);
    REQUIRE(store.save(core::State{}));
    fs::remove(dir.path / L"state.key");
    StateStore reopened(dir.path);
    CHECK_FALSE(reopened.load());
}

// ---------------------------------------------------------------------------------------------------
// Pipe (a private name for each test)

TEST_CASE("A pipe request gets its reply and identifies the caller", "[platform][pipe]") {
    Logger log = quiet_log();
    const std::wstring name = unique_pipe_name();
    PipeServer server(name, [](std::string_view request, const PipeClient& client) {
        return std::format("{}|{}|{}", request, client.pid, client.user_sid);
    }, log);
    REQUIRE(server.start());

    for (int i = 0; i < 3; ++i) {
        auto reply = pipe_call(name, "hello");
        REQUIRE(reply);
        CHECK(reply->starts_with(std::format("hello|{}|S-1-", GetCurrentProcessId())));
    }
}

TEST_CASE("Only one program can own a pipe name", "[platform][pipe][security]") {
    Logger log = quiet_log();
    const std::wstring name = unique_pipe_name();
    PipeServer server(name, [](std::string_view, const PipeClient&) { return std::string("real"); }, log);
    REQUIRE(server.start());
    PipeServer impostor(name, [](std::string_view, const PipeClient&) { return std::string("fake"); }, log);
    CHECK_FALSE(impostor.start());
}

TEST_CASE("A client can insist on who answers", "[platform][pipe][security]") {
    Logger log = quiet_log();
    const std::wstring name = unique_pipe_name();
    PipeServer server(name, [](std::string_view, const PipeClient&) { return std::string("ok"); }, log);
    REQUIRE(server.start());

    CHECK(pipe_call(name, "x", {std::chrono::seconds{5}, executable_path()}));
    const auto wrong = pipe_call(name, "x", {std::chrono::seconds{5}, fs::path(L"C:\\Elsewhere\\sirenproof-service.exe")});
    REQUIRE_FALSE(wrong);
    CHECK(wrong.error().find("something other than") != std::string::npos);
}

TEST_CASE("Oversized requests are refused and the server keeps working", "[platform][pipe]") {
    Logger log = quiet_log();
    const std::wstring name = unique_pipe_name();
    PipeServer server(name, [](std::string_view request, const PipeClient&) { return std::to_string(request.size()); }, log);
    REQUIRE(server.start());

    CHECK_FALSE(pipe_call(name, std::string(kMaxPipeMessage + 1, 'x')));
    auto at_limit = pipe_call(name, std::string(kMaxPipeMessage, 'x'));
    REQUIRE(at_limit);
    CHECK(*at_limit == std::to_string(kMaxPipeMessage));
}

TEST_CASE("Calling a pipe nobody serves says the service isn't running", "[platform][pipe]") {
    const auto reply = pipe_call(unique_pipe_name(), "x");
    REQUIRE_FALSE(reply);
    CHECK(reply.error().find("isn't running") != std::string::npos);
}

TEST_CASE("A stopped server stops answering", "[platform][pipe]") {
    Logger log = quiet_log();
    const std::wstring name = unique_pipe_name();
    PipeServer server(name, [](std::string_view, const PipeClient&) { return std::string("ok"); }, log);
    REQUIRE(server.start());
    REQUIRE(pipe_call(name, "x"));
    server.stop();
    CHECK_FALSE(pipe_call(name, "x", {std::chrono::milliseconds{500}, std::nullopt}));
}

// ---------------------------------------------------------------------------------------------------
// Processes and the app blocker (only the sleeper is ever closed)

TEST_CASE("inspect_process describes a running program", "[platform][process]") {
    const auto self = inspect_process(GetCurrentProcessId());
    REQUIRE(self);
    CHECK(fs::path(widen(self->identity.path)) == executable_path());
    if (in_desktop_session()) CHECK(self->session != 0);
    CHECK_FALSE(self->critical);
    const auto age = std::chrono::system_clock::now() - self->started;
    CHECK(age > std::chrono::seconds{-5});
    CHECK(age < std::chrono::hours{1});

    const auto listed = list_processes();
    CHECK(std::ranges::any_of(listed, [](const ProcessEntry& e) { return e.pid == GetCurrentProcessId(); }));
}

TEST_CASE("terminate_process checks it's still the same process", "[platform][process]") {
    Sleeper sleeper;
    const auto details = inspect_process(sleeper.info.dwProcessId);
    REQUIRE(details);
    CHECK_FALSE(terminate_process(sleeper.info.dwProcessId, details->started - std::chrono::seconds{1}));
    CHECK_FALSE(sleeper.exited_within(200));
    CHECK(terminate_process(sleeper.info.dwProcessId, details->started));
    CHECK(sleeper.exited_within(5000));
}

TEST_CASE("AppBlocker closes a program started while its block is on", "[platform][apps]") {
    if (!in_desktop_session()) SKIP("needs a desktop session: Sirenproof never closes session-0 services");
    Logger log = quiet_log();
    AppBlocker blocker(false, log);
    const auto pattern = core::AppPattern::parse(core::AppPattern::Kind::Path, SIRENPROOF_SLEEPER_EXE);
    REQUIRE(pattern);
    const auto decide = [&](const core::AppIdentity& app) {
        core::Verdict v;
        if (pattern->matches(app)) {
            v.action = core::Action::Block;
            v.rule_id = "games";
            v.strength = core::Strength::Block;
        }
        return v;
    };

    const auto block_began = std::chrono::system_clock::now() - std::chrono::seconds{30};
    blocker.sweep(decide, {"games"}, block_began);  // the block is on before the program starts

    Sleeper sleeper;
    CHECK(blocker.sweep(decide, {"games"}, std::chrono::system_clock::now()) == 1);
    CHECK(sleeper.exited_within(5000));
}

TEST_CASE("AppBlocker gives an already-running program time to save", "[platform][apps]") {
    if (!in_desktop_session()) SKIP("needs a desktop session: Sirenproof never closes session-0 services");
    Logger log = quiet_log();
    AppBlocker blocker(false, log, std::chrono::seconds{10});
    Sleeper sleeper;
    const auto pattern = core::AppPattern::parse(core::AppPattern::Kind::Path, SIRENPROOF_SLEEPER_EXE);
    REQUIRE(pattern);
    const auto decide = [&](const core::AppIdentity& app) {
        core::Verdict v;
        if (pattern->matches(app)) {
            v.action = core::Action::Block;
            v.rule_id = "games";
        }
        return v;
    };

    const auto now = std::chrono::system_clock::now() + std::chrono::seconds{1};  // the block begins after it started
    CHECK(blocker.sweep(decide, {"games"}, now) == 0);
    CHECK(blocker.sweep(decide, {"games"}, now + std::chrono::seconds{9}) == 0);
    CHECK_FALSE(sleeper.exited_within(100));
    CHECK(blocker.sweep(decide, {"games"}, now + std::chrono::seconds{10}) == 1);
    CHECK(sleeper.exited_within(5000));
}

TEST_CASE("AppBlocker never closes anything in dry-run mode", "[platform][apps]") {
    Logger log = quiet_log();
    AppBlocker blocker(true, log);
    Sleeper sleeper;
    const auto decide = [](const core::AppIdentity& app) {
        core::Verdict v;
        if (app.path.ends_with("sleeper.exe")) v.action = core::Action::Block;
        return v;
    };
    const auto now = std::chrono::system_clock::now();
    blocker.sweep(decide, {}, now);
    blocker.sweep(decide, {}, now + std::chrono::minutes{1});
    CHECK_FALSE(sleeper.exited_within(200));
}

TEST_CASE("AppBlocker never closes its own process, whatever a rule says", "[platform][apps][safety]") {
    // The "rule" targets only this test program, and the sweep is real. If the self-protection failed,
    // this process would be closed and the run would fail loudly.
    Logger log = quiet_log();
    AppBlocker blocker(false, log);
    const std::string self = core::normalise_app_path(narrow(executable_path().native()));
    const auto block_me = [&](const core::AppIdentity& app) {
        core::Verdict v;
        if (core::normalise_app_path(app.path) == self) {
            v.action = core::Action::Block;
            v.rule_id = "me";
        }
        return v;
    };
    const auto now = std::chrono::system_clock::now();
    CHECK(blocker.sweep(block_me, {"me"}, now - std::chrono::hours{1}) == 0);
    CHECK(blocker.sweep(block_me, {"me"}, now + std::chrono::hours{1}) == 0);
}

// ---------------------------------------------------------------------------------------------------
// Clock and randomness

TEST_CASE("read_clock reports wall time, a rising monotonic clock and a stable boot id", "[platform][clock]") {
    const core::ClockReading a = read_clock();
    Sleep(20);
    const core::ClockReading b = read_clock();
    const auto drift = std::chrono::abs(a.wall - std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()));
    CHECK(drift < std::chrono::seconds{2});
    CHECK(b.monotonic > a.monotonic);
    CHECK(a.boot_id == b.boot_id);
}

TEST_CASE("local_offset_at gives a plausible time zone offset", "[platform][clock]") {
    const auto offset = local_offset_at(std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
    CHECK(offset >= std::chrono::hours{-12});
    CHECK(offset <= std::chrono::hours{14});
    CHECK(offset.count() % 15 == 0);
}

TEST_CASE("SystemRandom produces different bytes each time", "[platform][random]") {
    SystemRandom random;
    std::array<std::uint8_t, 32> a{};
    std::array<std::uint8_t, 32> b{};
    random.fill(a);
    random.fill(b);
    CHECK(a != b);
}
