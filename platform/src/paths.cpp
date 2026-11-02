#include "sirenproof/platform/paths.hpp"

#include <string>

#include "sirenproof/platform/win.hpp"

namespace sirenproof::platform {

namespace {

std::wstring environment(const wchar_t* name, const wchar_t* fallback) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) return fallback;
    std::wstring value(size, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), size);
    value.resize(written);
    return value;
}

}  // namespace

std::filesystem::path program_data_dir() {
    return std::filesystem::path(environment(L"ProgramData", L"C:\\ProgramData")) / L"Sirenproof";
}

std::filesystem::path system_hosts_file() {
    wchar_t system[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(system, MAX_PATH);
    const std::filesystem::path dir = length ? std::filesystem::path(system) : L"C:\\Windows\\System32";
    return dir / L"drivers" / L"etc" / L"hosts";
}

std::filesystem::path executable_path() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length < buffer.size()) {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path executable_dir() { return executable_path().parent_path(); }

}  // namespace sirenproof::platform
