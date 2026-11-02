#include "sirenproof/platform/log.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>

namespace sirenproof::platform {

namespace {

constexpr std::uintmax_t kRotateAt = 1024 * 1024;
constexpr std::size_t kKeepInMemory = 300;

}  // namespace

Logger::Logger(std::filesystem::path file, bool echo_to_console) : file_(std::move(file)), echo_(echo_to_console) {}

void Logger::write(std::string_view level, std::string_view message) {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const std::string line = std::format("{:%FT%TZ} {:<5} {}", now, level, message);

    const std::scoped_lock lock(mutex_);
    recent_.push_back(line);
    if (recent_.size() > kKeepInMemory) recent_.pop_front();
    if (echo_) std::clog << line << '\n';
    if (file_.empty()) return;

    std::error_code ec;
    if (std::filesystem::exists(file_, ec) && std::filesystem::file_size(file_, ec) > kRotateAt) {
        std::filesystem::path old = file_;
        old += ".1";
        std::filesystem::rename(file_, old, ec);
    }
    std::ofstream out(file_, std::ios::app | std::ios::binary);
    out << line << "\r\n";
}

std::vector<std::string> Logger::recent(std::size_t count) const {
    const std::scoped_lock lock(mutex_);
    const std::size_t start = recent_.size() > count ? recent_.size() - count : 0;
    return {recent_.begin() + static_cast<std::ptrdiff_t>(start), recent_.end()};
}

}  // namespace sirenproof::platform
