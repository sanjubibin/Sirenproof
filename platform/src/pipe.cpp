#include "sirenproof/platform/pipe.hpp"

#include <sddl.h>

#include <cstdlib>
#include <format>
#include <memory>

namespace sirenproof::platform {

namespace {

using std::chrono::milliseconds;
using std::chrono::steady_clock;

struct LocalFreeDeleter {
    void operator()(void* p) const { LocalFree(p); }
};

DWORD to_ms(milliseconds ms) { return ms.count() <= 0 ? 0 : static_cast<DWORD>(ms.count()); }

// Waits for an overlapped operation. Returns its error code (ERROR_SUCCESS when it completed), cancelling
// it on timeout (WAIT_TIMEOUT) or when `stop` is signalled (ERROR_OPERATION_ABORTED).
// `timeout_ms` may be INFINITE.
DWORD finish_io(HANDLE file, OVERLAPPED& overlapped, BOOL started, HANDLE stop, DWORD timeout_ms, DWORD& bytes) {
    DWORD error = started ? ERROR_SUCCESS : GetLastError();
    if (!started && error == ERROR_IO_PENDING) {
        const HANDLE waits[] = {overlapped.hEvent, stop};
        const DWORD count = stop ? 2 : 1;
        const DWORD which = WaitForMultipleObjects(count, waits, FALSE, timeout_ms);
        if (which != WAIT_OBJECT_0) {
            CancelIoEx(file, &overlapped);
            GetOverlappedResult(file, &overlapped, &bytes, TRUE);
            return which == WAIT_TIMEOUT ? WAIT_TIMEOUT : ERROR_OPERATION_ABORTED;
        }
        error = GetOverlappedResult(file, &overlapped, &bytes, FALSE) ? ERROR_SUCCESS : GetLastError();
    } else if (started) {
        GetOverlappedResult(file, &overlapped, &bytes, FALSE);
    }
    return error;
}

std::string client_sid(HANDLE pipe) {
    if (!ImpersonateNamedPipeClient(pipe)) return {};
    std::string sid;
    HANDLE token = nullptr;
    if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
        Handle owned(token);
        alignas(TOKEN_USER) std::byte buffer[256];
        DWORD size = 0;
        if (GetTokenInformation(token, TokenUser, buffer, sizeof(buffer), &size)) {
            wchar_t* text = nullptr;
            if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid, &text)) {
                sid = narrow(text);
                LocalFree(text);
            }
        }
    }
    // Carrying on as the client would be a privilege escalation; never risk it.
    if (!RevertToSelf()) std::abort();
    return sid;
}

}  // namespace

std::optional<std::uint32_t> service_process_id() {
    const SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return std::nullopt;
    std::optional<std::uint32_t> pid;
    if (const SC_HANDLE service = OpenServiceW(scm, std::wstring(kServiceName).c_str(), SERVICE_QUERY_STATUS)) {
        SERVICE_STATUS_PROCESS status{};
        DWORD needed = 0;
        if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(&status), sizeof(status), &needed)) {
            pid = status.dwCurrentState == SERVICE_STOPPED ? 0 : status.dwProcessId;
        }
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
    return pid;
}

std::filesystem::path process_image_path(std::uint32_t pid) {
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process) return {};
    std::wstring path(32'768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process.get(), 0, path.data(), &length)) return {};
    path.resize(length);
    return path;
}

PipeServer::PipeServer(std::wstring name, PipeHandler handler, Logger& log)
    : name_(std::move(name)), handler_(std::move(handler)), log_(log), stop_event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}

PipeServer::~PipeServer() { stop(); }

std::expected<HANDLE, std::string> PipeServer::create_instance(bool first) {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(std::wstring(kPipeSddl).c_str(), SDDL_REVISION_1,
                                                              &descriptor, nullptr)) {
        return std::unexpected("invalid pipe security descriptor: " + last_error_text());
    }
    const std::unique_ptr<void, LocalFreeDeleter> owner(descriptor);
    SECURITY_ATTRIBUTES sa{sizeof(sa), descriptor, FALSE};

    const DWORD open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
    const DWORD pipe_mode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    const HANDLE pipe = CreateNamedPipeW(name_.c_str(), open_mode, pipe_mode, PIPE_UNLIMITED_INSTANCES,
                                         kMaxPipeMessage, kMaxPipeMessage, 0, &sa);
    if (pipe == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (first && error == ERROR_ACCESS_DENIED) {
            return std::unexpected("another program already owns the pipe " + narrow(name_));
        }
        return std::unexpected("can't create the pipe: " + error_text(error));
    }
    return pipe;
}

std::expected<void, std::string> PipeServer::start() {
    if (acceptor_.joinable()) return {};
    auto first = create_instance(true);
    if (!first) return std::unexpected(first.error());
    ResetEvent(stop_event_.get());
    acceptor_ = std::thread([this, pipe = *first] { accept_loop(pipe); });
    return {};
}

void PipeServer::stop() {
    if (!acceptor_.joinable()) return;
    SetEvent(stop_event_.get());
    acceptor_.join();
    // Client threads watch the same event; give them a moment to finish their current reply.
    for (int i = 0; i < 500 && active_.load() > 0; ++i) Sleep(10);
}

void PipeServer::accept_loop(HANDLE first) {
    Handle connected_event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    HANDLE next = first;
    for (;;) {
        Handle pipe(next);
        next = nullptr;
        if (!pipe) {
            auto created = create_instance(false);
            if (!created) {
                log_.error(created.error());
                if (WaitForSingleObject(stop_event_.get(), 1000) == WAIT_OBJECT_0) return;
                continue;
            }
            pipe.reset(*created);
        }

        OVERLAPPED overlapped{};
        overlapped.hEvent = connected_event.get();
        ResetEvent(connected_event.get());
        DWORD bytes = 0;
        const BOOL started = ConnectNamedPipe(pipe.get(), &overlapped);
        DWORD error = started ? ERROR_SUCCESS : GetLastError();
        if (error != ERROR_PIPE_CONNECTED) {
            error = finish_io(pipe.get(), overlapped, started, stop_event_.get(), INFINITE, bytes);
        } else {
            error = ERROR_SUCCESS;
        }
        if (WaitForSingleObject(stop_event_.get(), 0) == WAIT_OBJECT_0) return;
        if (error != ERROR_SUCCESS) continue;  // the client went away; make a fresh instance

        if (active_.load() >= kMaxClients) {
            log_.warn("too many pipe clients at once; dropping a connection");
            DisconnectNamedPipe(pipe.get());
            continue;
        }
        ++active_;
        std::thread([this, raw = pipe.release()] { serve(raw); }).detach();
    }
}

void PipeServer::serve(HANDLE raw) {
    Handle pipe(raw);
    Handle io_event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    PipeClient client;
    ULONG pid = 0;
    ULONG session = 0;
    if (GetNamedPipeClientProcessId(pipe.get(), &pid)) client.pid = pid;
    if (GetNamedPipeClientSessionId(pipe.get(), &session)) client.session = session;

    std::string buffer(kMaxPipeMessage, '\0');
    bool identified = false;
    for (;;) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = io_event.get();
        ResetEvent(io_event.get());
        DWORD read = 0;
        const BOOL started = ReadFile(pipe.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read, &overlapped);
        const DWORD error = finish_io(pipe.get(), overlapped, started, stop_event_.get(), to_ms(kIdleTimeout), read);
        if (error == ERROR_MORE_DATA) {
            log_.warn(std::format("dropped an oversized message from process {}", client.pid));
            break;
        }
        if (error != ERROR_SUCCESS) break;  // disconnected, idle or stopping

        if (!identified) {
            client.user_sid = client_sid(pipe.get());  // possible only after the first read
            identified = true;
        }

        std::string reply = handler_(std::string_view(buffer.data(), read), client);
        if (reply.size() > kMaxPipeMessage) reply = R"({"ok":false,"error":"the reply was too large"})";

        overlapped = OVERLAPPED{};
        overlapped.hEvent = io_event.get();
        ResetEvent(io_event.get());
        DWORD written = 0;
        const BOOL sent = WriteFile(pipe.get(), reply.data(), static_cast<DWORD>(reply.size()), &written, &overlapped);
        if (finish_io(pipe.get(), overlapped, sent, stop_event_.get(), 5000, written) != ERROR_SUCCESS) {
            break;
        }
    }
    DisconnectNamedPipe(pipe.get());
    --active_;
}

std::expected<std::string, std::string> pipe_call(std::wstring_view name, std::string_view request,
                                                  const PipeCallOptions& options) {
    if (request.size() > kMaxPipeMessage) return std::unexpected("the request is too large");
    const auto deadline = steady_clock::now() + options.timeout;
    const std::wstring pipe_name(name);
    const auto remaining = [&] { return std::chrono::duration_cast<milliseconds>(deadline - steady_clock::now()); };

    Handle pipe;
    for (;;) {
        pipe.reset(CreateFileW(pipe_name.c_str(), GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES, 0, nullptr,
                               OPEN_EXISTING, FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
                               nullptr));
        if (pipe) break;
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) return std::unexpected("the Sirenproof service isn't running");
        if (error != ERROR_PIPE_BUSY) return std::unexpected("can't connect to the service: " + error_text(error));
        if (remaining().count() <= 0 || !WaitNamedPipeW(pipe_name.c_str(), to_ms(remaining()))) {
            return std::unexpected("the service is busy; try again");
        }
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr)) {
        return std::unexpected("can't talk to the service: " + last_error_text());
    }

    if (options.expected_server) {
        const auto impostor = [] { return std::unexpected("something other than the Sirenproof service is answering on its pipe"); };
        ULONG server_pid = 0;
        if (!GetNamedPipeServerProcessId(pipe.get(), &server_pid)) return impostor();
        const bool service_pipe = CompareStringOrdinal(pipe_name.c_str(), -1, kServicePipeName.data(),
                                                       static_cast<int>(kServicePipeName.size()), TRUE) == CSTR_EQUAL;
        const auto installed = service_pipe ? service_process_id() : std::nullopt;
        if (installed) {
            // Windows says which process the service is; nothing else may answer for it.
            if (*installed == 0 || server_pid != *installed) return impostor();
        } else {
            const std::filesystem::path server = process_image_path(server_pid);
            const std::wstring& want = options.expected_server->native();
            if (server.empty() || CompareStringOrdinal(server.c_str(), -1, want.c_str(), -1, TRUE) != CSTR_EQUAL) {
                return impostor();
            }
        }
    }

    Handle io_event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    OVERLAPPED overlapped{};
    overlapped.hEvent = io_event.get();
    std::string reply(kMaxPipeMessage, '\0');
    DWORD read = 0;
    const BOOL started = TransactNamedPipe(pipe.get(), const_cast<char*>(request.data()), static_cast<DWORD>(request.size()),
                                           reply.data(), static_cast<DWORD>(reply.size()), &read, &overlapped);
    const DWORD error = finish_io(pipe.get(), overlapped, started, nullptr, to_ms(remaining()), read);
    if (error == WAIT_TIMEOUT) return std::unexpected("the service didn't answer in time");
    if (error == ERROR_MORE_DATA) return std::unexpected("the service's reply was too large");
    if (error != ERROR_SUCCESS) return std::unexpected("lost the connection to the service: " + error_text(error));
    reply.resize(read);
    return reply;
}

}  // namespace sirenproof::platform
