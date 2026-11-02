#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "sirenproof/core/domain.hpp"
#include "sirenproof/platform/log.hpp"
#include "sirenproof/platform/win.hpp"

namespace sirenproof::platform {

// Keeps Sirenproof's section of a hosts file in line with the sites that should be blocked.
// With dry_run it only logs what it would write.
class HostsFile {
public:
    HostsFile(std::filesystem::path path, bool dry_run, Logger& log);

    // Brings the file in line with `sites`. Returns true if it had to change it, whether because the
    // list changed or because someone edited the section.
    std::expected<bool, std::string> enforce(const std::vector<core::Domain>& sites);

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    bool dry_run_;
    Logger& log_;
    std::string last_dry_run_;  // avoids logging the same "would write" over and over
};

// Asks Windows' DNS client to forget cached answers, so a new block applies at once. Best effort:
// the function is undocumented, so it's looked up at run time.
void flush_dns_cache();

// Signals its event whenever a file in `dir` is written or renamed; call rearm() after each signal.
class DirectoryWatch {
public:
    explicit DirectoryWatch(const std::filesystem::path& dir);
    ~DirectoryWatch();
    DirectoryWatch(const DirectoryWatch&) = delete;
    DirectoryWatch& operator=(const DirectoryWatch&) = delete;

    HANDLE event() const { return handle_; }
    bool valid() const { return handle_ != INVALID_HANDLE_VALUE; }
    void rearm();

private:
    HANDLE handle_;
};

}  // namespace sirenproof::platform
