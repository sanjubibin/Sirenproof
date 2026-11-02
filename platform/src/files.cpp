#include "sirenproof/platform/files.hpp"

#include <aclapi.h>
#include <sddl.h>

#include <format>
#include <memory>

#include "sirenproof/platform/win.hpp"

namespace sirenproof::platform {

namespace {

std::expected<void, std::string> fail(std::string_view what, const std::filesystem::path& path) {
    return std::unexpected(std::format("{} {}: {}", what, narrow(path.native()), last_error_text()));
}

struct LocalFreeDeleter {
    void operator()(void* p) const { LocalFree(p); }
};

bool is_admin_or_system(PSID sid) {
    return IsWellKnownSid(sid, WinLocalSystemSid) || IsWellKnownSid(sid, WinBuiltinAdministratorsSid);
}

}  // namespace

std::expected<std::string, std::string> read_file(const std::filesystem::path& path) {
    Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) return std::unexpected(std::format("can't open {}: {}", narrow(path.native()), last_error_text()));

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart > 64 * 1024 * 1024) {
        return std::unexpected(std::format("{} is unreadable or too large", narrow(path.native())));
    }
    std::string content(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!content.empty() && !ReadFile(file.get(), content.data(), static_cast<DWORD>(content.size()), &read, nullptr)) {
        return std::unexpected(std::format("can't read {}: {}", narrow(path.native()), last_error_text()));
    }
    content.resize(read);
    return content;
}

std::expected<void, std::string> write_file_atomic(const std::filesystem::path& path, std::string_view content) {
    std::filesystem::path temp = path;
    temp += std::format(L".sirenproof-{}.tmp", GetCurrentProcessId());
    {
        Handle file(CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file) return fail("can't create", temp);
        DWORD written = 0;
        const bool ok = WriteFile(file.get(), content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
                        written == content.size() && FlushFileBuffers(file.get());
        if (!ok) {
            auto error = fail("can't write", temp);
            file.reset();
            DeleteFileW(temp.c_str());
            return error;
        }
    }

    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {  // new file: just move it into place
        if (MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return {};
        auto error = fail("can't create", path);
        DeleteFileW(temp.c_str());
        return error;
    }

    // ReplaceFileW keeps the existing file's permissions, but refuses read-only files, so lift that briefly.
    const bool read_only = (attributes & FILE_ATTRIBUTE_READONLY) != 0;
    if (read_only) SetFileAttributesW(path.c_str(), attributes & ~DWORD{FILE_ATTRIBUTE_READONLY});
    const bool replaced = ReplaceFileW(path.c_str(), temp.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr);
    const DWORD error_code = GetLastError();
    if (read_only) SetFileAttributesW(path.c_str(), attributes);
    if (!replaced) {
        DeleteFileW(temp.c_str());
        return std::unexpected(std::format("can't replace {}: {}", narrow(path.native()), error_text(error_code)));
    }
    return {};
}

std::expected<void, std::string> ensure_trusted_directory(const std::filesystem::path& dir,
                                                          std::optional<std::wstring_view> sddl) {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (sddl && !ConvertStringSecurityDescriptorToSecurityDescriptorW(std::wstring(*sddl).c_str(), SDDL_REVISION_1,
                                                                      &descriptor, nullptr)) {
        return std::unexpected("invalid security descriptor: " + last_error_text());
    }
    const std::unique_ptr<void, LocalFreeDeleter> descriptor_owner(descriptor);

    const DWORD attributes = GetFileAttributesW(dir.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        if (dir.has_parent_path()) {
            std::error_code ignored;
            std::filesystem::create_directories(dir.parent_path(), ignored);
        }
        SECURITY_ATTRIBUTES sa{sizeof(sa), descriptor, FALSE};
        if (!CreateDirectoryW(dir.c_str(), descriptor ? &sa : nullptr)) return fail("can't create", dir);
        return {};
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return std::unexpected(std::format("{} exists but isn't a folder", narrow(dir.native())));
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        return std::unexpected(std::format("{} is a link to somewhere else; refusing to use it", narrow(dir.native())));
    }
    if (!sddl) return {};

    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR current = nullptr;
    if (GetNamedSecurityInfoW(dir.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr,
                              nullptr, &current) != ERROR_SUCCESS) {
        return fail("can't read the owner of", dir);
    }
    const std::unique_ptr<void, LocalFreeDeleter> current_owner(current);
    if (!is_admin_or_system(owner)) {
        return std::unexpected(std::format(
            "{} was created by an ordinary user, so it can't be trusted; delete it and start again",
            narrow(dir.native())));
    }

    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    PACL dacl = nullptr;
    GetSecurityDescriptorDacl(descriptor, &present, &dacl, &defaulted);
    std::wstring path = dir.native();
    if (SetNamedSecurityInfoW(path.data(), SE_FILE_OBJECT,
                              DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, dacl,
                              nullptr) != ERROR_SUCCESS) {
        return fail("can't set permissions on", dir);
    }
    return {};
}

}  // namespace sirenproof::platform
