#include <chrono>

#include "sirenproof/platform/system.hpp"
#include "sirenproof/platform/win.hpp"

namespace sirenproof::platform {

namespace {

// FILETIME counts 100 ns intervals since 1601; the Unix epoch is 11644473600 seconds later.
constexpr std::int64_t kFileTimeToUnixSeconds = 11'644'473'600;

std::chrono::sys_time<std::chrono::milliseconds> from_filetime(const FILETIME& ft) {
    const std::int64_t ticks =
        static_cast<std::int64_t>((static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime);
    return std::chrono::sys_time<std::chrono::milliseconds>{
        std::chrono::milliseconds{ticks / 10'000 - kFileTimeToUnixSeconds * 1000}};
}

FILETIME to_filetime(core::TimePoint t) {
    const std::int64_t ticks = (t.time_since_epoch().count() + kFileTimeToUnixSeconds) * 10'000'000;
    const auto u = static_cast<std::uint64_t>(ticks);
    return FILETIME{static_cast<DWORD>(u & 0xffffffffu), static_cast<DWORD>(u >> 32)};
}

// Time since boot including sleep. QueryInterruptTime (Windows 10+) isn't in MinGW's kernel32 import
// library, so it's looked up at run time; GetTickCount64 also counts sleep and is the fallback.
std::chrono::milliseconds monotonic_now() {
    using QueryInterruptTimeFn = VOID(WINAPI*)(PULONGLONG);
    static const auto query = [] {
        const HMODULE kernelbase = GetModuleHandleW(L"kernelbase.dll");
        return kernelbase ? reinterpret_cast<QueryInterruptTimeFn>(
                                reinterpret_cast<void*>(GetProcAddress(kernelbase, "QueryInterruptTime")))
                          : nullptr;
    }();
    if (query) {
        ULONGLONG hundred_ns = 0;
        query(&hundred_ns);
        return std::chrono::milliseconds{static_cast<std::int64_t>(hundred_ns / 10'000)};
    }
    return std::chrono::milliseconds{static_cast<std::int64_t>(GetTickCount64())};
}

// Windows counts boots in the registry; any change means the monotonic clock restarted.
std::uint64_t boot_id() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management\\PrefetchParameters",
        L"BootId", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return status == ERROR_SUCCESS ? value : 0;
}

}  // namespace

core::ClockReading read_clock() {
    FILETIME now{};
    GetSystemTimePreciseAsFileTime(&now);
    return core::ClockReading{from_filetime(now), monotonic_now(), boot_id()};
}

core::Minutes local_offset_at(core::TimePoint t) {
    const FILETIME utc_ft = to_filetime(t);
    SYSTEMTIME utc{};
    SYSTEMTIME local{};
    DYNAMIC_TIME_ZONE_INFORMATION zone{};
    if (!FileTimeToSystemTime(&utc_ft, &utc) || GetDynamicTimeZoneInformation(&zone) == TIME_ZONE_ID_INVALID ||
        !SystemTimeToTzSpecificLocalTimeEx(&zone, &utc, &local)) {
        return core::Minutes{0};
    }
    FILETIME local_ft{};
    if (!SystemTimeToFileTime(&local, &local_ft)) return core::Minutes{0};
    return std::chrono::duration_cast<core::Minutes>(from_filetime(local_ft) - from_filetime(utc_ft));
}

}  // namespace sirenproof::platform
