#pragma once

// Helpers for the Windows-layer tests. Everything they create is temporary and private to the test.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <format>
#include <string>

#include "sirenproof/platform/files.hpp"
#include "sirenproof/platform/log.hpp"
#include "sirenproof/platform/win.hpp"

namespace sirenproof::test {

namespace fs = std::filesystem;

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

inline std::string read(const fs::path& p) {
    auto text = platform::read_file(p);
    REQUIRE(text.has_value());
    return *text;
}

inline std::wstring unique_pipe_name() {
    static std::atomic<int> counter{0};
    return std::format(L"\\\\.\\pipe\\Sirenproof.test-{}-{}", GetCurrentProcessId(), ++counter);
}

inline platform::Logger quiet_log() { return platform::Logger({}, false); }

// CI machines run tests as an elevated administrator, sometimes as a service in session 0; a few tests
// only mean something for an ordinary user at a desktop.
inline bool running_elevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) &&
                          elevation.TokenIsElevated != 0;
    CloseHandle(token);
    return elevated;
}

inline bool in_desktop_session() {
    DWORD session = 0;
    return ProcessIdToSessionId(GetCurrentProcessId(), &session) && session != 0;
}

// The sleeper test program, a harmless stand-in for a "distracting app"; closed by the test or here.
struct Sleeper {
    PROCESS_INFORMATION info{};
    Sleeper() {
        std::wstring command = L"\"" + platform::widen(SIRENPROOF_SLEEPER_EXE) + L"\"";
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
    Sleeper(const Sleeper&) = delete;
    Sleeper& operator=(const Sleeper&) = delete;
    bool exited_within(DWORD ms) const { return WaitForSingleObject(info.hProcess, ms) == WAIT_OBJECT_0; }
};

}  // namespace sirenproof::test
