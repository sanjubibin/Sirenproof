#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "sirenproof/platform/log.hpp"
#include "sirenproof/platform/win.hpp"

namespace sirenproof::platform {

inline constexpr std::wstring_view kServicePipeName = L"\\\\.\\pipe\\Sirenproof.v1";
inline constexpr std::size_t kMaxPipeMessage = 64 * 1024;

// SYSTEM, Administrators and the pipe's owner: full access. People signed in at the PC (IU): read and
// write messages (0x0012018b), but not FILE_CREATE_PIPE_INSTANCE, so no other program can open its own
// end of the pipe and pose as the service.
inline constexpr std::wstring_view kPipeSddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;OW)(A;;0x0012018b;;;IU)";

struct PipeClient {
    std::uint32_t pid = 0;
    std::uint32_t session = 0;
    std::string user_sid;  // the connecting account, e.g. "S-1-5-21-..."
};

// Answers one request. Called on a per-connection thread.
using PipeHandler = std::function<std::string(std::string_view request, const PipeClient& client)>;

// A message-mode named pipe server. Each message is one request; each reply one message. Local
// clients only; messages over kMaxPipeMessage, idle connections and too many clients are dropped.
class PipeServer {
public:
    PipeServer(std::wstring name, PipeHandler handler, Logger& log);
    ~PipeServer();
    PipeServer(const PipeServer&) = delete;
    PipeServer& operator=(const PipeServer&) = delete;

    // Fails if another process already owns a pipe with this name.
    std::expected<void, std::string> start();
    void stop();

    static constexpr int kMaxClients = 16;
    static constexpr std::chrono::seconds kIdleTimeout{30};

private:
    std::expected<HANDLE, std::string> create_instance(bool first);
    void accept_loop(HANDLE first);
    void serve(HANDLE pipe);

    std::wstring name_;
    PipeHandler handler_;
    Logger& log_;
    Handle stop_event_;
    std::thread acceptor_;
    std::atomic<int> active_{0};
};

struct PipeCallOptions {
    std::chrono::milliseconds timeout{5000};
    // If set, the reply is only accepted when the program serving the pipe is this exe.
    std::optional<std::filesystem::path> expected_server;
};

// Sends one request and waits for the reply. Connects with identification-only rights, so the server
// can see who is asking but can't act as them.
std::expected<std::string, std::string> pipe_call(std::wstring_view name, std::string_view request,
                                                  const PipeCallOptions& options = {});

// Full path of the program running process `pid`, or empty.
std::filesystem::path process_image_path(std::uint32_t pid);

}  // namespace sirenproof::platform
