#include "sirenproof/platform/process.hpp"

#include "sirenproof/platform/win.hpp"

#include <appmodel.h>
#include <softpub.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <algorithm>
#include <cwctype>
#include <format>

#include "sirenproof/core/browser.hpp"

namespace sirenproof::platform {

namespace {

// True if `path` is a file on a drive of this PC, reached without a symbolic link: only then may the
// service (as SYSTEM) open it. A share (\\server\x.exe), a network drive, or a link pointing at one
// would have Windows sign in to that server as this PC, and could stall the service while it waits.
// Mount points stay on this PC, so they're fine. (Someone swapping a folder for a link between this
// check and the read could still slip past; that takes the right to create links, which ordinary
// accounts don't have outside Developer Mode.)
bool is_local_file(const std::wstring& path) {
    if (path.size() < 4 || !std::iswalpha(path[0]) || path[1] != L':' || path[2] != L'\\') return false;
    // No wildcards, streams (x.exe:more) or . and .. folders, so the checks below see the real path.
    if (path.find_first_of(L"*?\"<>|") != std::wstring::npos || path.find(L':', 2) != std::wstring::npos) return false;
    if (path.find(L"\\.\\") != std::wstring::npos || path.find(L"\\..\\") != std::wstring::npos ||
        path.ends_with(L"\\.") || path.ends_with(L"\\..")) {
        return false;
    }
    const wchar_t root[] = {path[0], L':', L'\\', L'\0'};
    switch (GetDriveTypeW(root)) {
        case DRIVE_FIXED:
        case DRIVE_REMOVABLE:
        case DRIVE_CDROM:
        case DRIVE_RAMDISK: break;
        default: return false;
    }
    // Each folder on the way, then the file itself, as its parent folder lists it (without following it).
    for (std::size_t end = path.find(L'\\', 3);; end = path.find(L'\\', end + 1)) {
        const std::wstring part = path.substr(0, end);
        WIN32_FIND_DATAW found{};
        const HANDLE find = FindFirstFileExW(part.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);
        if (find == INVALID_HANDLE_VALUE) return false;
        FindClose(find);
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && found.dwReserved0 != IO_REPARSE_TAG_MOUNT_POINT) {
            return false;
        }
        if (end == std::wstring::npos) return true;
    }
}

ProcessStart from_filetime(const FILETIME& ft) {
    const auto ticks = static_cast<std::int64_t>((static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime);
    return ProcessStart{std::chrono::milliseconds{ticks / 10'000 - 11'644'473'600'000}};
}

std::string original_filename(const std::wstring& path) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0) return {};
    std::vector<std::byte> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) return {};

    struct Translation {
        WORD language;
        WORD codepage;
    };
    Translation* translations = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &length) ||
        length < sizeof(Translation)) {
        return {};
    }
    const std::wstring key = std::format(L"\\StringFileInfo\\{:04x}{:04x}\\OriginalFilename",
                                         translations[0].language, translations[0].codepage);
    wchar_t* value = nullptr;
    if (!VerQueryValueW(data.data(), key.c_str(), reinterpret_cast<void**>(&value), &length) || length == 0) return {};
    return narrow(std::wstring_view(value, length > 0 && value[length - 1] == L'\0' ? length - 1 : length));
}

// The signer of a validly signed file, or empty. Revocation isn't checked and nothing is downloaded.
std::string publisher(const std::wstring& path) {
    WINTRUST_FILE_INFO file{};
    file.cbStruct = sizeof(file);
    file.pcwszFilePath = path.c_str();

    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_REVOCATION_CHECK_NONE;

    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    std::string name;
    if (WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data) == ERROR_SUCCESS) {
        CRYPT_PROVIDER_DATA* provider = WTHelperProvDataFromStateData(data.hWVTStateData);
        CRYPT_PROVIDER_SGNR* signer = provider ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : nullptr;
        if (signer && signer->csCertChain > 0 && signer->pasCertChain[0].pCert) {
            wchar_t buffer[256] = {};
            if (CertGetNameStringW(signer->pasCertChain[0].pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, buffer,
                                   256) > 1) {
                name = narrow(buffer);
            }
        }
    }
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);
    return name;
}

}  // namespace

std::vector<ProcessEntry> list_processes() {
    std::vector<ProcessEntry> out;
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot) return out;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Process32FirstW(snapshot.get(), &entry); more; more = Process32NextW(snapshot.get(), &entry)) {
        out.push_back({entry.th32ProcessID, entry.th32ParentProcessID, entry.szExeFile});
    }
    return out;
}

std::optional<std::filesystem::path> browser_ancestor(std::uint32_t pid) {
    const std::vector<ProcessEntry> processes = list_processes();
    const auto find = [&](std::uint32_t id) -> const ProcessEntry* {
        const auto it = std::ranges::find(processes, id, &ProcessEntry::pid);
        return it == processes.end() ? nullptr : &*it;
    };

    const ProcessEntry* child = find(pid);
    for (int level = 0; child && level < 4; ++level) {
        const ProcessEntry* parent = find(child->parent);
        if (!parent || parent->pid == child->pid) return std::nullopt;
        const auto child_details = inspect_process(child->pid);
        const auto parent_details = inspect_process(parent->pid);
        if (!child_details || !parent_details || parent_details->started > child_details->started) return std::nullopt;

        const std::string name = narrow(parent->exe_name);
        if (core::browser_for_exe(name)) return std::filesystem::path(widen(parent_details->identity.path));
        if (CompareStringOrdinal(parent->exe_name.c_str(), -1, L"cmd.exe", -1, TRUE) != CSTR_EQUAL &&
            CompareStringOrdinal(parent->exe_name.c_str(), -1, L"conhost.exe", -1, TRUE) != CSTR_EQUAL) {
            return std::nullopt;
        }
        child = parent;
    }
    return std::nullopt;
}

std::optional<ProcessDetails> inspect_process(std::uint32_t pid) {
    if (pid == 0 || pid == 4) return std::nullopt;  // the idle and System processes
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process) return std::nullopt;

    ProcessDetails details;
    details.pid = pid;

    std::wstring path(MAX_PATH, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    while (!QueryFullProcessImageNameW(process.get(), 0, path.data(), &length)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || path.size() > 32'768) return std::nullopt;
        path.resize(path.size() * 2);
        length = static_cast<DWORD>(path.size());
    }
    path.resize(length);
    details.identity.path = narrow(path);

    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(process.get(), &created, &exited, &kernel, &user)) return std::nullopt;
    details.started = from_filetime(created);

    DWORD session = 0;
    if (ProcessIdToSessionId(pid, &session)) details.session = session;

    BOOL critical = FALSE;
    if (IsProcessCritical(process.get(), &critical)) details.critical = critical != FALSE;

    UINT32 family_length = 0;
    if (GetPackageFamilyName(process.get(), &family_length, nullptr) == ERROR_INSUFFICIENT_BUFFER && family_length > 0) {
        std::wstring family(family_length, L'\0');
        if (GetPackageFamilyName(process.get(), &family_length, family.data()) == ERROR_SUCCESS) {
            family.resize(family_length > 0 ? family_length - 1 : 0);
            details.identity.package_family = narrow(family);
        }
    }
    return details;
}

bool terminate_process(std::uint32_t pid, ProcessStart started) {
    Handle process(OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process) return false;
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(process.get(), &created, &exited, &kernel, &user) || from_filetime(created) != started) {
        return false;  // a different process now has this id
    }
    return TerminateProcess(process.get(), 1) != FALSE;
}

void AppIdentityCache::complete(core::AppIdentity& identity) {
    const std::wstring path = widen(identity.path);
    if (!is_local_file(path)) return;  // matched by its name and path only
    std::error_code ec;
    const auto written = std::filesystem::last_write_time(path, ec);
    if (ec) return;

    auto it = entries_.find(path);
    if (it == entries_.end() || it->second.written != written) {
        if (entries_.size() > 2000) entries_.clear();  // plenty for one PC; keeps memory bounded
        it = entries_.insert_or_assign(path, Entry{written, original_filename(path), publisher(path)}).first;
    }
    identity.original_filename = it->second.original_filename;
    identity.publisher = it->second.publisher;
}

}  // namespace sirenproof::platform
