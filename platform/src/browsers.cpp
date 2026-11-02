#include "sirenproof/platform/browsers.hpp"

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <format>

#include "sirenproof/core/browser.hpp"
#include "sirenproof/core/json_codec.hpp"
#include "sirenproof/core/crypto.hpp"
#include "sirenproof/platform/files.hpp"
#include "sirenproof/platform/process.hpp"

namespace sirenproof::platform {

namespace {

std::wstring lower(std::wstring text) {
    for (wchar_t& c : text) c = static_cast<wchar_t>(std::towlower(c));
    return text;
}

std::vector<std::uint8_t> dword(DWORD value) {
    return {static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 24)};
}

std::vector<std::uint8_t> wide_string(std::wstring_view text) {
    std::vector<std::uint8_t> bytes((text.size() + 1) * sizeof(wchar_t), 0);
    std::memcpy(bytes.data(), text.data(), text.size() * sizeof(wchar_t));
    return bytes;
}

struct Current {
    bool exists = false;
    DWORD type = 0;
    std::vector<std::uint8_t> data;
};

Current read_value(HKEY root, const std::wstring& key, const std::wstring& name) {
    Current current;
    DWORD type = 0;
    DWORD size = 0;
    if (RegGetValueW(root, key.c_str(), name.c_str(), RRF_RT_ANY | RRF_NOEXPAND, &type, nullptr, &size) != ERROR_SUCCESS) {
        return current;
    }
    current.data.resize(size);
    if (RegGetValueW(root, key.c_str(), name.c_str(), RRF_RT_ANY | RRF_NOEXPAND, &type, current.data.data(), &size) !=
        ERROR_SUCCESS) {
        return Current{};
    }
    current.data.resize(size);
    current.exists = true;
    current.type = type;
    return current;
}

std::expected<void, std::string> write_value(HKEY root, const std::wstring& key, const std::wstring& name, DWORD type,
                                             const std::vector<std::uint8_t>& data) {
    HKEY handle = nullptr;
    if (const LSTATUS s = RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handle, nullptr);
        s != ERROR_SUCCESS) {
        return std::unexpected("can't open " + narrow(key) + ": " + error_text(static_cast<DWORD>(s)));
    }
    const LSTATUS s = RegSetValueExW(handle, name.c_str(), 0, type, data.data(), static_cast<DWORD>(data.size()));
    RegCloseKey(handle);
    if (s != ERROR_SUCCESS) return std::unexpected("can't set " + narrow(name) + ": " + error_text(static_cast<DWORD>(s)));
    return {};
}

void delete_value(HKEY root, const std::wstring& key, const std::wstring& name) {
    HKEY handle = nullptr;
    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_SET_VALUE, &handle) == ERROR_SUCCESS) {
        RegDeleteValueW(handle, name.c_str());
        RegCloseKey(handle);
    }
}

}  // namespace

// ---------------------------------------------------------------------------------------------------
// BrowserPolicies

BrowserPolicies::BrowserPolicies(HKEY root, std::wstring policies_path, std::filesystem::path ledger, bool dry_run,
                                 Logger& log)
    : root_(root), policies_path_(std::move(policies_path)), ledger_(std::move(ledger)), dry_run_(dry_run), log_(log) {}

std::vector<BrowserPolicies::Wanted> BrowserPolicies::wanted() const {
    const std::wstring& base = policies_path_;
    std::vector<Wanted> out;
    for (const auto& [key, private_mode] : {std::pair{L"\\Google\\Chrome", L"IncognitoModeAvailability"},
                                           std::pair{L"\\Microsoft\\Edge", L"InPrivateModeAvailability"},
                                           std::pair{L"\\BraveSoftware\\Brave", L"IncognitoModeAvailability"}}) {
        out.push_back({base + key, private_mode, REG_DWORD, dword(1)});  // 1 = private windows unavailable
        out.push_back({base + key, L"DnsOverHttpsMode", REG_SZ, wide_string(L"off")});
    }
    return out;
}

std::expected<void, std::string> BrowserPolicies::set(bool on) {
    if (dry_run_) {
        if (dry_run_state_ != on) {
            log_.info(on ? "dry run: would turn off private windows and DNS-over-HTTPS in Chrome, Edge and Brave"
                         : "dry run: would restore browser policies");
            dry_run_state_ = on;
        }
        return {};
    }

    // The ledger: {"values": [{"key", "name", "previous": null | {"type", "data"}}]}
    core::Json ledger = core::Json::object();
    if (auto text = read_file(ledger_)) {
        ledger = core::Json::parse(*text, nullptr, false);
        if (ledger.is_discarded() || !ledger.is_object()) ledger = core::Json::object();
    }
    if (!ledger.contains("values") || !ledger["values"].is_array()) ledger["values"] = core::Json::array();
    auto& values = ledger["values"];
    const auto recorded = [&](const std::wstring& key, const std::wstring& name) {
        return std::ranges::any_of(values, [&](const core::Json& v) {
            return v.value("key", "") == narrow(key) && v.value("name", "") == narrow(name);
        });
    };

    if (!on) {
        if (values.empty()) return {};
        for (const core::Json& v : values) {
            const std::wstring key = widen(v.value("key", ""));
            const std::wstring name = widen(v.value("name", ""));
            if (v["previous"].is_object()) {
                const auto data = core::from_hex(v["previous"].value("data", ""));
                if (!data) continue;
                if (auto restored = write_value(root_, key, name, v["previous"].value("type", DWORD{REG_SZ}), *data); !restored) {
                    log_.warn("couldn't restore a browser policy: " + restored.error());
                }
            } else {
                delete_value(root_, key, name);
            }
        }
        std::error_code ignored;
        std::filesystem::remove(ledger_, ignored);
        log_.info("browser policies restored");
        return {};
    }

    bool changed = false;
    for (const Wanted& w : wanted()) {
        const Current current = read_value(root_, w.key, w.name);
        if (current.exists && current.type == w.type && current.data == w.data) continue;
        if (!recorded(w.key, w.name)) {
            core::Json previous = nullptr;
            if (current.exists) previous = core::Json{{"type", current.type}, {"data", core::to_hex(current.data)}};
            values.push_back({{"key", narrow(w.key)}, {"name", narrow(w.name)}, {"previous", previous}});
            // Record before changing anything, so a crash can't lose the original value.
            if (auto saved = write_file_atomic(ledger_, ledger.dump(2)); !saved) return saved;
        }
        if (auto written = write_value(root_, w.key, w.name, w.type, w.data); !written) return written;
        changed = true;
    }
    if (changed) log_.info("private windows and DNS-over-HTTPS turned off in Chrome, Edge and Brave for the lock");
    return {};
}

// ---------------------------------------------------------------------------------------------------
// BrowserGuard

BrowserGuard::BrowserGuard(bool dry_run, Logger& log, std::chrono::seconds grace, std::chrono::seconds startup_window)
    : dry_run_(dry_run), log_(log), grace_(grace), startup_window_(startup_window) {}

void BrowserGuard::heartbeat(const std::filesystem::path& browser, Clock::time_point now) {
    const std::wstring key = lower(browser.native());
    heartbeats_[key] = now;
    if (close_at_.erase(key) > 0) log_.info(std::format("{}: Sirenproof's extension connected", narrow(browser.native())));
}

bool BrowserGuard::is_browser(const std::wstring& exe_name, bool& extension_available) const {
    if (!test_names_.empty()) {
        extension_available = true;
        return std::ranges::any_of(test_names_, [&](const std::wstring& n) {
            return CompareStringOrdinal(n.c_str(), -1, exe_name.c_str(), -1, TRUE) == CSTR_EQUAL;
        });
    }
    const auto info = core::browser_for_exe(narrow(exe_name));
    if (!info) return false;
    extension_available = info->extension_available;
    return true;
}

int BrowserGuard::enforce(Clock::time_point now) {
    const auto wall_now = std::chrono::system_clock::now();
    int closed = 0;
    for (const ProcessEntry& entry : list_processes()) {
        bool extension_available = false;
        if (!is_browser(entry.exe_name, extension_available)) continue;
        const auto details = inspect_process(entry.pid);
        if (!details || details->session == 0 || details->critical) continue;

        const std::wstring key = lower(widen(details->identity.path));
        display_names_[key] = details->identity.path;
        const auto beat = heartbeats_.find(key);
        if (extension_available && beat != heartbeats_.end() && now - beat->second < kHeartbeatTimeout) continue;
        if (wall_now - details->started < startup_window_) continue;  // give a new browser time to connect

        auto deadline = close_at_.find(key);
        if (deadline == close_at_.end()) {
            deadline = close_at_.emplace(key, now + grace_).first;
            log_.warn(std::format("{} is open without Sirenproof's extension{}; it will be closed in {} s",
                                  details->identity.path, extension_available ? "" : " (not supported yet)",
                                  grace_.count()));
        }
        if (now < deadline->second) continue;

        if (dry_run_) {
            log_.info(std::format("dry run: would close {}", details->identity.path));
            continue;
        }
        if (terminate_process(details->pid, details->started)) ++closed;
    }
    if (closed > 0) log_.info(std::format("closed {} browser process(es) running without the extension", closed));
    return closed;
}

std::vector<std::string> BrowserGuard::unprotected() const {
    std::vector<std::string> out;
    for (const auto& [key, when] : close_at_) out.push_back(display_names_.contains(key) ? display_names_.at(key) : narrow(key));
    return out;
}

}  // namespace sirenproof::platform
