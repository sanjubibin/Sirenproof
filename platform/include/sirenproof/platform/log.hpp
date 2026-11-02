#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace sirenproof::platform {

// A small thread-safe log: a file in the data folder (rotated at 1 MiB), optional echo to the console,
// and the last few hundred lines kept in memory for `sirenproof status`. Never leaves the machine.
class Logger {
public:
    // An empty path logs to the console only.
    Logger(std::filesystem::path file, bool echo_to_console);

    void info(std::string_view message) { write("info", message); }
    void warn(std::string_view message) { write("warn", message); }
    void error(std::string_view message) { write("error", message); }

    std::vector<std::string> recent(std::size_t count) const;

private:
    void write(std::string_view level, std::string_view message);

    std::filesystem::path file_;
    bool echo_;
    mutable std::mutex mutex_;
    std::deque<std::string> recent_;
};

}  // namespace sirenproof::platform
