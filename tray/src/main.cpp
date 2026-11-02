// sirenproof-tray: the tray icon. Shows whether a lock holds, opens the main window, offers a break,
// says when a lock starts or ends, and shows the pause when a Nudge rule catches a program. It doesn't
// poll: a thread waits for the service to say something changed (events.wait), and otherwise it sleeps.
//
//   sirenproof-tray [--pipe <name>]
//   sirenproof-tray --self-test [--pipe <name>]   add the icon, ask the service once, remove it, exit
//   sirenproof-tray --pause-preview <file.png> [--elapsed <seconds>]   draw the pause into a file
//
// Checking the budgets in plan §3.7 (development; they write JSON and exit):
//   sirenproof-tray --measure-idle <file> [--seconds <n>]   CPU used while idle, and memory
//   sirenproof-tray --measure-pause <file>   from "something changed" to the pause's first frame, with
//                                            the pause invisible and click-through

#include "sirenproof/platform/win.hpp"

#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "pause_overlay.hpp"
#include "sirenproof/core/json_codec.hpp"
#include "sirenproof/core/time.hpp"
#include "sirenproof/platform/paths.hpp"
#include "sirenproof/platform/pipe.hpp"
#include "sirenproof/platform/system.hpp"

#include <psapi.h>

using namespace sirenproof;

namespace {

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kChangedMessage = WM_APP + 2;  // from the watcher thread: ask the service again
constexpr UINT kIconId = 1;
enum Command : UINT { kOpen = 1, kBreak, kQuit };

struct TrayState {
    HWND window = nullptr;
    HICON icon = nullptr;
    UINT taskbar_created = 0;
    std::wstring pipe = std::wstring(platform::kServicePipeName);
    std::wstring tip = L"Sirenproof";
    std::optional<std::string> lock_until;  // end of the lock that holds, if one does
    HANDLE ui_process = nullptr;
    DWORD ui_pid = 0;
    std::unique_ptr<tray::PauseOverlay> pause;
};
TrayState g_tray;

// Budget checks (see the top of this file).
constexpr UINT_PTR kMeasureStart = 1;
constexpr UINT_PTR kMeasureEnd = 2;
struct Measurement {
    std::wstring idle_file;
    int idle_seconds = 60;
    double cpu_at_start_ms = 0;
    std::wstring pause_file;
    std::uint64_t changed_at_ms = 0;
    core::Json steps = core::Json::object();  // milliseconds after changed_at_ms
};
Measurement g_measure;

std::uint64_t unix_ms() {
    FILETIME now{};
    GetSystemTimePreciseAsFileTime(&now);
    const std::uint64_t ticks = (std::uint64_t{now.dwHighDateTime} << 32) | now.dwLowDateTime;
    return (ticks - 116'444'736'000'000'000ULL) / 10'000;
}

double cpu_ms() {
    FILETIME created{}, ended{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user);
    const auto value = [](const FILETIME& t) { return (std::uint64_t{t.dwHighDateTime} << 32) | t.dwLowDateTime; };
    return static_cast<double>(value(kernel) + value(user)) / 10'000.0;
}

void write_measurement(const std::wstring& file, const core::Json& result) {
    const std::string text = result.dump();
    HANDLE out = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(out, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(out);
}

void finish_idle_measurement() {
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory));
    write_measurement(g_measure.idle_file,
                      {{"seconds", g_measure.idle_seconds},
                       {"cpu_ms", cpu_ms() - g_measure.cpu_at_start_ms},
                       {"private_mb", static_cast<double>(memory.PrivateUsage) / (1024.0 * 1024.0)},
                       {"working_set_mb", static_cast<double>(memory.WorkingSetSize) / (1024.0 * 1024.0)}});
    DestroyWindow(g_tray.window);
}

void pause_step(const char* step) {
    if (!g_measure.pause_file.empty()) g_measure.steps[step] = unix_ms() - g_measure.changed_at_ms;
}

void pause_first_frame() {
    if (g_measure.pause_file.empty()) return;
    const std::uint64_t now = unix_ms();
    write_measurement(g_measure.pause_file, {{"changed_at_ms", g_measure.changed_at_ms},
                                             {"frame_at_ms", now},
                                             {"changed_to_frame_ms", now - g_measure.changed_at_ms},
                                             {"steps", g_measure.steps}});
    DestroyWindow(g_tray.window);
}

// The icon, drawn in code: a sea-glass ring with the mast through it.
HICON make_icon(int size) {
    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = size;
    header.bV5Height = -size;  // top-down
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00ff0000;
    header.bV5GreenMask = 0x0000ff00;
    header.bV5BlueMask = 0x000000ff;
    header.bV5AlphaMask = 0xff000000;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color || !bits) return nullptr;

    const auto clamp01 = [](double v) { return std::clamp(v, 0.0, 1.0); };
    const double centre = (size - 1) / 2.0;
    const double outer = size * 0.46;
    const double inner = size * 0.32;
    const double mast = size * 0.065;
    auto* pixels = static_cast<std::uint32_t*>(bits);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const double dx = x - centre;
            const double dy = y - centre;
            const double d = std::sqrt(dx * dx + dy * dy);
            const double ring = clamp01(outer + 0.5 - d) * clamp01(d - inner + 0.5);
            const double pole = clamp01(mast + 0.5 - std::abs(dx)) * clamp01(outer * 0.78 + 0.5 - std::abs(dy));
            const double a = std::max(ring, pole);
            const auto channel = [&](double c) { return static_cast<std::uint32_t>(std::lround(c * a)); };
            pixels[y * size + x] = (static_cast<std::uint32_t>(std::lround(255 * a)) << 24) | (channel(0x5c) << 16) |
                                   (channel(0xe1) << 8) | channel(0xe6);
        }
    }

    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO info{TRUE, 0, 0, mask, color};
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

std::optional<core::Json> ask(std::string_view op, core::Json args = core::Json::object()) {
    const core::Json request{{"v", 1}, {"op", op}, {"args", std::move(args)}};
    const auto reply = platform::pipe_call(g_tray.pipe, request.dump(),
                                           {std::chrono::seconds{2}, platform::executable_dir() / L"sirenproof-service.exe"});
    if (!reply) return std::nullopt;
    core::Json answer = core::Json::parse(*reply, nullptr, false);
    if (answer.is_discarded() || !answer.value("ok", false)) return std::nullopt;
    return answer["result"];
}

std::wstring local_clock(const std::string& iso) {
    const auto t = core::parse_time(iso);
    if (!t) return L"?";
    return platform::widen(std::format("{:%H:%M}", *t + platform::local_offset_at(*t)));
}

NOTIFYICONDATAW icon_data() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = g_tray.window;
    data.uID = kIconId;
    return data;
}

void notify(const std::wstring& title, const std::wstring& text) {
    NOTIFYICONDATAW data = icon_data();
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
    title.copy(data.szInfoTitle, std::size(data.szInfoTitle) - 1);
    text.copy(data.szInfo, std::size(data.szInfo) - 1);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

bool add_icon() {
    NOTIFYICONDATAW data = icon_data();
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    data.uCallbackMessage = kTrayMessage;
    data.hIcon = g_tray.icon;
    g_tray.tip.copy(data.szTip, std::size(data.szTip) - 1);
    if (!Shell_NotifyIconW(NIM_ADD, &data)) return false;
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    return true;
}

void refresh(bool announce) {
    const auto status = ask("status");
    pause_step("status");
    std::optional<std::string> lock;
    std::wstring tip;
    if (!status) {
        tip = L"Sirenproof: the service isn't running";
    } else if (const auto& locked = (*status)["locked"]; locked.is_array() && !locked.empty()) {
        lock = locked[0].value("until", std::string{});
        tip = L"Sirenproof: locked until " + local_clock(*lock);
    } else {
        tip = (*status).value("dry_run", false) ? L"Sirenproof: dry run, nothing is enforced" : L"Sirenproof: enforcing";
    }

    if (announce && lock.has_value() != g_tray.lock_until.has_value()) {
        if (lock) notify(L"Locked", L"You tied yourself to the mast until " + local_clock(*lock) + L".");
        else notify(L"The lock has ended", L"Your scheduled rules still apply.");
    }
    g_tray.lock_until = lock;
    g_tray.tip = tip;

    NOTIFYICONDATAW data = icon_data();
    data.uFlags = NIF_TIP | NIF_SHOWTIP;
    tip.copy(data.szTip, std::size(data.szTip) - 1);
    Shell_NotifyIconW(NIM_MODIFY, &data);

    // The pause: shown for the first program waiting at it, gone when none is (or the service is).
    if (!g_tray.pause) return;
    const core::Json paused = status ? (*status).value("paused", core::Json::array()) : core::Json::array();
    if (!paused.is_array() || paused.empty()) {
        g_tray.pause->hide();
        return;
    }
    const core::Json& first = paused[0];
    tray::PauseView view;
    view.id = first.value("id", std::string{});
    view.rule = first.value("rule", std::string{});
    std::wstring program = std::filesystem::path(platform::widen(first.value("program", std::string{}))).stem().native();
    if (!program.empty()) program[0] = static_cast<wchar_t>(towupper(program[0]));
    view.program = program;
    view.rule_name = platform::widen(first.value("rule_name", std::string{}));
    view.goal = platform::widen(first.value("goal", std::string{}));
    view.pause_seconds = first.value("pause_seconds", 15);
    view.continue_minutes = first.value("continue_minutes", 5);
    g_tray.pause->show(view, std::chrono::seconds{first.value("waited_seconds", 0)});
}

// "Close it" or "Continue" at the pause. The service does it; the next change hides the pause.
void choose_at_pause(const tray::PauseView& view, bool close) {
    const auto result = close ? ask("paused.close", {{"id", view.id}}) : ask("nudge.continue", {{"rule", view.rule}});
    if (!result) {
        notify(L"Sirenproof", L"The service didn't answer.");
    } else if (!(*result).value("ok", false)) {
        notify(L"Sirenproof", platform::widen((*result).value("message", std::string("That didn't work."))));
    }
    g_tray.pause->hide();
    refresh(false);
}

// Waits for the service to say something changed, and tells the window. When the service isn't
// running it tries again every few seconds; otherwise nothing happens until something changes.
void watch_service(HWND window, std::wstring pipe) {
    std::uint64_t seen = 0;
    bool reachable = true;
    for (;;) {
        const core::Json request{{"v", 1}, {"op", "events.wait"}, {"args", {{"after", seen}}}};
        const auto reply = platform::pipe_call(pipe, request.dump(),
                                               {std::chrono::seconds{35}, platform::executable_dir() / L"sirenproof-service.exe"});
        const core::Json answer = reply ? core::Json::parse(*reply, nullptr, false) : core::Json();
        if (answer.is_object() && answer.value("ok", false)) {
            const auto seq = answer["result"].value("seq", seen);
            if (seq != seen || !reachable) PostMessageW(window, kChangedMessage, 0, 0);
            seen = seq;
            reachable = true;
        } else {
            if (reachable) PostMessageW(window, kChangedMessage, 0, 0);
            reachable = false;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }
}

BOOL CALLBACK focus_if_ui(HWND window, LPARAM) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != g_tray.ui_pid || !IsWindowVisible(window) || GetWindow(window, GW_OWNER)) return TRUE;
    if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
    SetForegroundWindow(window);
    return FALSE;
}

void open_window() {
    if (g_tray.ui_process && WaitForSingleObject(g_tray.ui_process, 0) == WAIT_TIMEOUT) {
        EnumWindows(focus_if_ui, 0);
        return;
    }
    if (g_tray.ui_process) CloseHandle(g_tray.ui_process);
    g_tray.ui_process = nullptr;

    std::wstring command = L"\"" + (platform::executable_dir() / L"sirenproof-ui.exe").native() + L"\"";
    if (g_tray.pipe != platform::kServicePipeName) command += L" --pipe \"" + g_tray.pipe + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &info)) {
        CloseHandle(info.hThread);
        g_tray.ui_process = info.hProcess;
        g_tray.ui_pid = info.dwProcessId;
    } else {
        notify(L"Sirenproof", L"The main window couldn't be opened.");
    }
}

void take_break() {
    if (const auto result = ask("break.start")) {
        notify(L"Break", platform::widen((*result).value("ok", false) ? std::string("Five minutes. It ends on its own.")
                                                                       : (*result).value("message", std::string("Not possible right now."))));
    } else {
        notify(L"Break", L"The service isn't running.");
    }
}

void show_menu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, g_tray.tip.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kOpen, L"Open Sirenproof");
    AppendMenuW(menu, MF_STRING, kBreak, L"Take a 5-minute break");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kQuit, L"Hide this icon (blocking carries on)");
    SetMenuDefaultItem(menu, kOpen, FALSE);

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(g_tray.window);  // so the menu closes when you click elsewhere
    const UINT chosen = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                                                         cursor.x, cursor.y, 0, g_tray.window, nullptr));
    DestroyMenu(menu);
    PostMessageW(g_tray.window, WM_NULL, 0, 0);

    if (chosen == kOpen) open_window();
    if (chosen == kBreak) take_break();
    if (chosen == kQuit) DestroyWindow(g_tray.window);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == kTrayMessage) {
        switch (LOWORD(lparam)) {
            case NIN_SELECT:
            case NIN_KEYSELECT:
                open_window();
                return 0;
            case WM_CONTEXTMENU:
                show_menu();
                return 0;
            default:
                return 0;
        }
    }
    if (message == g_tray.taskbar_created && message != 0) {  // Explorer restarted: put the icon back
        add_icon();
        return 0;
    }
    switch (message) {
        case kChangedMessage:
            g_measure.changed_at_ms = unix_ms();
            refresh(true);
            return 0;
        case WM_TIMER:
            KillTimer(window, wparam);
            if (wparam == kMeasureStart) {
                g_measure.cpu_at_start_ms = cpu_ms();
                SetTimer(window, kMeasureEnd, static_cast<UINT>(g_measure.idle_seconds) * 1000, nullptr);
            }
            if (wparam == kMeasureEnd) finish_idle_measurement();
            return 0;
        case WM_DESTROY: {
            NOTIFYICONDATAW data = icon_data();
            Shell_NotifyIconW(NIM_DELETE, &data);
            PostQuitMessage(0);
            return 0;
        }
        default:
            return DefWindowProcW(window, message, wparam, lparam);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);  // a crisp pause on any screen

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool self_test = false;
    std::wstring preview;
    double elapsed = 5;
    for (int i = 1; i < argc; ++i) {
        const std::wstring_view arg = argv[i];
        if (arg == L"--self-test") self_test = true;
        if (arg == L"--pipe" && i + 1 < argc) g_tray.pipe = argv[++i];
        if (arg == L"--pause-preview" && i + 1 < argc) preview = argv[++i];
        if (arg == L"--elapsed" && i + 1 < argc) elapsed = _wtof(argv[++i]);
        if (arg == L"--measure-idle" && i + 1 < argc) g_measure.idle_file = argv[++i];
        if (arg == L"--seconds" && i + 1 < argc) g_measure.idle_seconds = std::max(1, _wtoi(argv[++i]));
        if (arg == L"--measure-pause" && i + 1 < argc) g_measure.pause_file = argv[++i];
    }
    LocalFree(argv);

    if (!preview.empty()) {
        tray::PauseView view;
        view.id = "n1";
        view.program = L"Steam";
        view.rule_name = L"School nights";
        view.goal = L"Finish the history essay";
        return tray::render_pause_png(view, elapsed, 1280, 800, preview) ? 0 : 1;
    }

    // One tray icon per signed-in user.
    HANDLE single = CreateMutexW(nullptr, TRUE, L"Local\\SirenproofTray");
    if (!self_test && GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"SirenproofTray";
    RegisterClassExW(&window_class);
    g_tray.window = CreateWindowExW(0, window_class.lpszClassName, L"Sirenproof", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                    instance, nullptr);
    g_tray.taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    g_tray.icon = make_icon(GetSystemMetrics(SM_CXSMICON) * 2);
    if (!g_tray.window || !g_tray.icon || !add_icon()) return 1;
    refresh(false);

    if (self_test) {
        // Say what the tooltip reads, for whoever started us with standard output redirected.
        const std::string line = platform::narrow(g_tray.tip) + "\n";
        DWORD written = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        DestroyWindow(g_tray.window);
        return 0;
    }

    g_tray.pause = std::make_unique<tray::PauseOverlay>(
        instance, choose_at_pause,
        tray::PauseOverlay::Measuring{!g_measure.pause_file.empty(), pause_first_frame, pause_step});
    // Let start-up settle before measuring the quiet that follows.
    if (!g_measure.idle_file.empty()) SetTimer(g_tray.window, kMeasureStart, 3000, nullptr);
    // The watcher lives as long as the program; it holds nothing that needs tidying up.
    std::thread(watch_service, g_tray.window, g_tray.pipe).detach();

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    g_tray.pause.reset();
    if (g_tray.ui_process) CloseHandle(g_tray.ui_process);
    DestroyIcon(g_tray.icon);
    if (single) CloseHandle(single);
    return 0;
}
