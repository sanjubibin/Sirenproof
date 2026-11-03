// sirenproof-host: the bridge between the browser extension and the Sirenproof service.
//
// The browser starts it with the extension's origin as the first argument and talks to it over
// stdin/stdout with native messaging framing. Each message is checked, only extension operations are
// passed on, and only to the real service. The relay keeps no state and can't change anything itself.
//
//   sirenproof-host chrome-extension://<id>/        started by the browser
//   sirenproof-host register [--user]               tell Chrome, Edge and Brave where this host is
//   sirenproof-host unregister [--user]
//   sirenproof-host relay --pipe <name> [--trust-any-server]     for tests

#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "sirenproof/core/browser.hpp"
#include "sirenproof/core/json_codec.hpp"
#include "sirenproof/platform/native_host.hpp"
#include "sirenproof/platform/paths.hpp"
#include "sirenproof/platform/pipe.hpp"

using namespace sirenproof;
using core::Json;

namespace {

bool read_exact(HANDLE in, void* buffer, DWORD size) {
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    while (size > 0) {
        DWORD read = 0;
        if (!ReadFile(in, bytes, size, &read, nullptr) || read == 0) return false;
        bytes += read;
        size -= read;
    }
    return true;
}

bool write_all(HANDLE out, std::string_view data) {
    while (!data.empty()) {
        DWORD written = 0;
        if (!WriteFile(out, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) || written == 0) return false;
        data.remove_prefix(written);
    }
    return true;
}

std::string handle_message(std::string_view body, const std::wstring& pipe,
                           const std::optional<std::filesystem::path>& expected_server) {
    if (core::nested_deeper_than(body, core::kMaxMessageDepth)) return Json{{"id", nullptr}, {"ok", false}, {"error", "malformed message"}}.dump();
    const Json message = Json::parse(body, nullptr, false);
    const Json id = message.is_object() && message.contains("id") ? message["id"] : Json(nullptr);
    const auto error = [&](std::string_view why) { return Json{{"id", id}, {"ok", false}, {"error", std::string(why)}}.dump(); };

    if (!message.is_object() || !message.contains("op") || !message["op"].is_string()) return error("malformed message");
    const std::string op = message["op"].get<std::string>();
    if (!core::is_extension_op(op)) return error("the extension may not ask for " + op);
    const Json args = message.contains("args") ? message["args"] : Json::object();
    if (!args.is_object()) return error("args must be an object");

    const Json request{{"v", 1}, {"op", op}, {"args", args}};
    const auto reply = platform::pipe_call(pipe, request.dump(), {std::chrono::seconds{3}, expected_server});
    if (!reply) return error(reply.error());

    Json answer = Json::parse(*reply, nullptr, false);
    if (answer.is_discarded() || !answer.is_object()) return error("the service sent something unreadable");
    answer["id"] = id;
    std::string text = answer.dump();
    if (text.size() > core::kMaxToBrowser) return error("the reply was too large");
    return text;
}

int relay(const std::wstring& pipe, const std::optional<std::filesystem::path>& expected_server) {
    const HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    for (;;) {
        std::array<std::uint8_t, 4> header{};
        if (!read_exact(in, header.data(), 4)) return 0;  // the browser closed the connection
        const std::uint32_t length = core::decode_length(header);
        if (length > core::kMaxFromBrowser) return 1;  // not something our extension would send
        std::string body(length, '\0');
        if (length > 0 && !read_exact(in, body.data(), length)) return 0;
        if (!write_all(out, core::frame_message(handle_message(body, pipe, expected_server)))) return 0;
    }
}

bool has(const std::vector<std::wstring>& args, const wchar_t* flag) {
    for (const auto& a : args) {
        if (a == flag) return true;
    }
    return false;
}

std::wstring value_of(const std::vector<std::wstring>& args, const wchar_t* flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) return args[i + 1];
    }
    return {};
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    const std::vector<std::wstring> args(argv + 1, argv + argc);
    const std::filesystem::path service = platform::executable_dir() / L"sirenproof-service.exe";

    if (!args.empty() && core::is_our_extension(platform::narrow(args[0]))) {
        return relay(std::wstring(platform::kServicePipeName), service);
    }
    if (!args.empty() && (args[0] == L"register" || args[0] == L"unregister")) {
        const HKEY root = has(args, L"--user") ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
        if (args[0] == L"unregister") {
            platform::unregister_native_host(root);
            std::puts("Unregistered.");
            return 0;
        }
        if (auto ok = platform::register_native_host(root, platform::executable_path()); !ok) {
            std::fprintf(stderr, "sirenproof-host: %s\n", ok.error().c_str());
            return 1;
        }
        std::puts("Registered with Chrome, Edge and Brave.");
        return 0;
    }
    if (!args.empty() && args[0] == L"relay") {
        const std::wstring pipe = value_of(args, L"--pipe");
        if (pipe.empty()) return 2;
        return relay(pipe, has(args, L"--trust-any-server") ? std::nullopt : std::optional(service));
    }

    std::fputs("sirenproof-host is started by the browser for Sirenproof's extension.\n"
               "  sirenproof-host register [--user]     tell Chrome, Edge and Brave where it is\n"
               "  sirenproof-host unregister [--user]\n",
               stderr);
    return 2;
}
