#pragma once

#include <windows.h>

#include <string>
#include <string_view>
#include <utility>

namespace sirenproof::platform {

// Owns a Windows HANDLE and closes it. Treats both nullptr and INVALID_HANDLE_VALUE as empty.
class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE h) : h_(h) {}
    ~Handle() { reset(); }
    Handle(Handle&& other) noexcept : h_(std::exchange(other.h_, nullptr)) {}
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset();
            h_ = std::exchange(other.h_, nullptr);
        }
        return *this;
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    HANDLE get() const noexcept { return h_; }
    bool valid() const noexcept { return h_ != nullptr && h_ != INVALID_HANDLE_VALUE; }
    explicit operator bool() const noexcept { return valid(); }
    void reset(HANDLE h = nullptr) noexcept {
        if (valid()) CloseHandle(h_);
        h_ = h;
    }
    [[nodiscard]] HANDLE release() noexcept { return std::exchange(h_, nullptr); }

private:
    HANDLE h_ = nullptr;
};

// UTF-8 <-> UTF-16, and readable error text: "Access is denied. (error 5)".
std::wstring widen(std::string_view utf8);
std::string narrow(std::wstring_view wide);
std::string error_text(DWORD code);
inline std::string last_error_text() { return error_text(GetLastError()); }

}  // namespace sirenproof::platform
