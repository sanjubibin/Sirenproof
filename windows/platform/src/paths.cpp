#include "sirenproof/platform/paths.hpp"

#include <string>

#include "sirenproof/platform/win.hpp"

#include <shlobj.h>

namespace sirenproof::platform {

namespace {

// Where Windows keeps a known folder, from the registry the system set up, not from environment
// variables, which whoever starts a program can change.
std::filesystem::path known_folder(const KNOWNFOLDERID& id, const wchar_t* fallback) {
    PWSTR path = nullptr;
    std::filesystem::path result = fallback;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &path)) && path) result = path;
    CoTaskMemFree(path);
    return result;
}

}  // namespace

std::filesystem::path program_data_dir() {
    return known_folder(FOLDERID_ProgramData, L"C:\\ProgramData") / L"Sirenproof";
}

std::vector<std::filesystem::path> program_files_dirs() {
    return {known_folder(FOLDERID_ProgramFiles, L"C:\\Program Files"),
            known_folder(FOLDERID_ProgramFilesX86, L"C:\\Program Files (x86)")};
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
