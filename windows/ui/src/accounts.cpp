#include "accounts.hpp"

#include <QStringList>

#include "sirenproof/platform/accounts.hpp"
#include "sirenproof/platform/win.hpp"

#include <shellapi.h>

namespace sirenproof::ui {

namespace {

bool shell_open(const wchar_t* file, const wchar_t* parameters) {
    const auto result = ShellExecuteW(nullptr, L"open", file, parameters, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

}  // namespace

QVariantMap Accounts::read() const {
    const auto admins = platform::read_administrators();
    if (!admins) return {{"error", QString::fromStdString(admins.error())}};
    QStringList others;
    for (const std::wstring& name : admins->others) others.push_back(QString::fromStdWString(name));
    return {{"user", QString::fromStdWString(admins->current_user)},
            {"admin", admins->current_is_admin},
            {"others", others}};
}

bool Accounts::openAccountSettings() const { return shell_open(L"ms-settings:otherusers", nullptr); }

bool Accounts::openUserAccounts() const { return shell_open(L"control.exe", L"/name Microsoft.UserAccounts"); }

}  // namespace sirenproof::ui
