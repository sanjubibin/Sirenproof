#include "sirenproof/platform/hosts_file.hpp"

#include <format>

#include "sirenproof/core/hosts.hpp"
#include "sirenproof/platform/files.hpp"

namespace sirenproof::platform {

HostsFile::HostsFile(std::filesystem::path path, bool dry_run, Logger& log)
    : path_(std::move(path)), dry_run_(dry_run), log_(log) {}

std::expected<bool, std::string> HostsFile::enforce(const std::vector<core::Domain>& sites) {
    std::string current;
    if (std::filesystem::exists(path_)) {
        auto text = read_file(path_);
        if (!text) return std::unexpected(text.error());
        current = *std::move(text);
    }

    const std::string wanted = core::apply_hosts_section(current, sites);
    if (wanted == current) return false;

    if (dry_run_) {
        if (wanted != last_dry_run_) {
            log_.info(std::format("dry run: would update {} to block {} site(s)", narrow(path_.native()), sites.size()));
            last_dry_run_ = wanted;
        }
        return true;
    }
    if (auto written = write_file_atomic(path_, wanted); !written) return std::unexpected(written.error());
    flush_dns_cache();
    log_.info(std::format("updated {}: {} site(s) blocked", narrow(path_.native()), sites.size()));
    return true;
}

void flush_dns_cache() {
    using FlushFn = BOOL(WINAPI*)();
    static const FlushFn flush = [] {
        HMODULE dnsapi = LoadLibraryExW(L"dnsapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        return dnsapi ? reinterpret_cast<FlushFn>(reinterpret_cast<void*>(GetProcAddress(dnsapi, "DnsFlushResolverCache")))
                      : nullptr;
    }();
    if (flush) flush();
}

DirectoryWatch::DirectoryWatch(const std::filesystem::path& dir)
    : handle_(FindFirstChangeNotificationW(dir.c_str(), FALSE,
                                           FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME |
                                               FILE_NOTIFY_CHANGE_SIZE)) {}

DirectoryWatch::~DirectoryWatch() {
    if (valid()) FindCloseChangeNotification(handle_);
}

void DirectoryWatch::rearm() {
    if (valid()) FindNextChangeNotification(handle_);
}

}  // namespace sirenproof::platform
