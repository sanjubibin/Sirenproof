#include "sirenproof/platform/accounts.hpp"

#include "sirenproof/platform/win.hpp"

#include <lm.h>

#include <cstddef>
#include <string_view>

namespace sirenproof::platform {

namespace {

std::string failure(std::string_view what, DWORD code) {
    return std::string(what) + " (Windows error " + std::to_string(code) + ")";
}

std::wstring account_name(PSID sid) {
    wchar_t name[256] = {};
    wchar_t domain[256] = {};
    DWORD name_length = 256;
    DWORD domain_length = 256;
    SID_NAME_USE use{};
    if (!LookupAccountSidW(nullptr, sid, name, &name_length, domain, &domain_length, &use)) return {};
    return name;
}

}  // namespace

std::expected<Administrators, std::string> read_administrators() {
    // The group's name depends on the Windows language, so find it by its well-known SID.
    BYTE admins_sid[SECURITY_MAX_SID_SIZE] = {};
    DWORD sid_size = sizeof(admins_sid);
    if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admins_sid, &sid_size)) {
        return std::unexpected(failure("couldn't name the Administrators group", GetLastError()));
    }
    const std::wstring group = account_name(admins_sid);
    if (group.empty()) return std::unexpected(failure("couldn't find the Administrators group", GetLastError()));

    // This program's own account.
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return std::unexpected(failure("couldn't read this account", GetLastError()));
    }
    DWORD needed = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    std::vector<std::byte> user_buffer(needed);
    const bool have_user = needed > 0 && GetTokenInformation(token, TokenUser, user_buffer.data(), needed, &needed);
    CloseHandle(token);
    if (!have_user) return std::unexpected(failure("couldn't read this account", GetLastError()));
    PSID user_sid = reinterpret_cast<TOKEN_USER*>(user_buffer.data())->User.Sid;

    Administrators result;
    result.current_user = account_name(user_sid);

    wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD computer_length = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(computer, &computer_length);

    LOCALGROUP_MEMBERS_INFO_2* members = nullptr;
    DWORD read = 0;
    DWORD total = 0;
    const NET_API_STATUS status = NetLocalGroupGetMembers(nullptr, group.c_str(), 2, reinterpret_cast<LPBYTE*>(&members),
                                                          MAX_PREFERRED_LENGTH, &read, &total, nullptr);
    if (status != NERR_Success) return std::unexpected(failure("couldn't list the administrators", status));

    for (DWORD i = 0; i < read; ++i) {
        const LOCALGROUP_MEMBERS_INFO_2& member = members[i];
        if (member.lgrmi2_sidusage != SidTypeUser) continue;
        if (EqualSid(member.lgrmi2_sid, user_sid)) {
            result.current_is_admin = true;
            continue;
        }
        // "PC-NAME\Keeper" for an account on this PC; another domain's accounts are listed as they are.
        const std::wstring_view full = member.lgrmi2_domainandname;
        const std::size_t slash = full.find(L'\\');
        const std::wstring name(slash == std::wstring_view::npos ? full : full.substr(slash + 1));
        const bool local = slash != std::wstring_view::npos && _wcsicmp(std::wstring(full.substr(0, slash)).c_str(), computer) == 0;
        if (local) {
            USER_INFO_1* info = nullptr;
            if (NetUserGetInfo(nullptr, name.c_str(), 1, reinterpret_cast<LPBYTE*>(&info)) == NERR_Success) {
                const bool disabled = (info->usri1_flags & UF_ACCOUNTDISABLE) != 0;
                NetApiBufferFree(info);
                if (disabled) continue;  // e.g. the built-in Administrator, off by default
            }
        }
        result.others.push_back(local ? name : std::wstring(full));
    }
    NetApiBufferFree(members);
    return result;
}

}  // namespace sirenproof::platform
