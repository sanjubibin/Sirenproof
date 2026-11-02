#include "sirenproof/platform/native_host.hpp"

#include "sirenproof/core/browser.hpp"
#include "sirenproof/core/json_codec.hpp"
#include "sirenproof/platform/files.hpp"

namespace sirenproof::platform {

namespace {

constexpr const wchar_t* kBrowserKeys[] = {
    L"Software\\Google\\Chrome\\NativeMessagingHosts\\",
    L"Software\\Microsoft\\Edge\\NativeMessagingHosts\\",
    L"Software\\BraveSoftware\\Brave-Browser\\NativeMessagingHosts\\",
};

}  // namespace

std::expected<void, std::string> register_native_host(HKEY root, const std::filesystem::path& host_exe) {
    core::Json origins = core::Json::array();
    for (const std::string_view id : core::kExtensionIds) origins.push_back("chrome-extension://" + std::string(id) + "/");
    const core::Json manifest{{"name", narrow(kNativeHostName)},
                              {"description", "Sirenproof: lets the browser extension ask the service what is blocked"},
                              {"path", narrow(host_exe.native())},
                              {"type", "stdio"},
                              {"allowed_origins", origins}};
    const std::filesystem::path manifest_path = host_exe.parent_path() / L"sirenproof-host.json";
    if (auto written = write_file_atomic(manifest_path, manifest.dump(2)); !written) return written;

    const std::wstring value = manifest_path.native();
    for (const wchar_t* base : kBrowserKeys) {
        const std::wstring key = std::wstring(base) + std::wstring(kNativeHostName);
        HKEY handle = nullptr;
        LSTATUS status = RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);
        if (status == ERROR_SUCCESS) {
            status = RegSetValueExW(handle, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                                    static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(handle);
        }
        if (status != ERROR_SUCCESS) return std::unexpected("can't register with the browsers: " + error_text(static_cast<DWORD>(status)));
    }
    return {};
}

void unregister_native_host(HKEY root) {
    for (const wchar_t* base : kBrowserKeys) {
        const std::wstring key = std::wstring(base) + std::wstring(kNativeHostName);
        RegDeleteKeyW(root, key.c_str());
    }
}

}  // namespace sirenproof::platform
