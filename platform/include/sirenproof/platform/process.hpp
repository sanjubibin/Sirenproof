#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "sirenproof/core/app.hpp"

namespace sirenproof::platform {

using ProcessStart = std::chrono::sys_time<std::chrono::milliseconds>;

struct ProcessEntry {
    std::uint32_t pid = 0;
    std::uint32_t parent = 0;
    std::wstring exe_name;
};

struct ProcessDetails {
    std::uint32_t pid = 0;
    std::uint32_t session = 0;
    ProcessStart started{};  // with the pid, identifies this exact process (pids get reused)
    bool critical = false;   // Windows would crash if it ended
    core::AppIdentity identity;
};

// Every running process (a cheap snapshot: ids and exe names only).
std::vector<ProcessEntry> list_processes();

// Path, session, start time and package of one process; nullopt if it's gone or can't be opened.
// Publisher and original filename are filled in by AppIdentityCache.
std::optional<ProcessDetails> inspect_process(std::uint32_t pid);

// Ends the process, but only if it's still the one that started at `started`.
bool terminate_process(std::uint32_t pid, ProcessStart started);

// The browser that started process `pid`, directly or through cmd.exe (Chrome launches native hosts
// that way), as the path of its exe. A parent only counts if it started before its child, so a reused
// process id can't pass for it.
std::optional<std::filesystem::path> browser_ancestor(std::uint32_t pid);

// Original filename (version resource) and publisher (a valid Authenticode signature) of an exe.
// Both are slow to read, so they're cached per path and last-write time. Signature checks never go
// to the network.
class AppIdentityCache {
public:
    void complete(core::AppIdentity& identity);

private:
    struct Entry {
        std::filesystem::file_time_type written;
        std::string original_filename;
        std::string publisher;
    };
    std::map<std::wstring, Entry> entries_;
};

}  // namespace sirenproof::platform
