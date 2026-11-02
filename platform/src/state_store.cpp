#include "sirenproof/platform/state_store.hpp"

#include "sirenproof/platform/win.hpp"

#include <dpapi.h>

#include <array>
#include <format>
#include <span>

#include "sirenproof/core/crypto.hpp"
#include "sirenproof/core/state_json.hpp"
#include "sirenproof/platform/files.hpp"
#include "sirenproof/platform/system.hpp"

namespace sirenproof::platform {

namespace {

constexpr std::string_view kHeader = "SIRENPROOF-STATE 1";

std::span<const std::uint8_t> bytes(std::string_view s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}

std::expected<std::string, std::string> dpapi(std::string_view input, bool protect) {
    DATA_BLOB in{static_cast<DWORD>(input.size()), const_cast<BYTE*>(reinterpret_cast<const BYTE*>(input.data()))};
    DATA_BLOB out{};
    const BOOL ok = protect ? CryptProtectData(&in, L"Sirenproof state key", nullptr, nullptr, nullptr,
                                               CRYPTPROTECT_UI_FORBIDDEN, &out)
                            : CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out);
    if (!ok) return std::unexpected(last_error_text());
    std::string result(reinterpret_cast<const char*>(out.pbData), out.cbData);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return result;
}

}  // namespace

StateStore::StateStore(std::filesystem::path dir) : dir_(std::move(dir)) {}

std::expected<std::string, std::string> StateStore::key() {
    if (!key_.empty()) return key_;
    const std::filesystem::path file = dir_ / L"state.key";

    if (auto sealed = read_file(file)) {
        auto opened = dpapi(*sealed, false);
        if (!opened || opened->size() != 32) return std::unexpected("the state key can't be opened by this account");
        key_ = *std::move(opened);
        return key_;
    }
    if (std::filesystem::exists(dir_ / L"state.dat")) {
        return std::unexpected("state.key is missing, so the saved state can't be trusted");
    }

    std::string fresh(32, '\0');
    SystemRandom{}.fill(std::span{reinterpret_cast<std::uint8_t*>(fresh.data()), fresh.size()});
    auto sealed = dpapi(fresh, true);
    if (!sealed) return std::unexpected("can't protect the state key: " + sealed.error());
    if (auto written = write_file_atomic(file, *sealed); !written) return std::unexpected(written.error());
    key_ = std::move(fresh);
    return key_;
}

std::expected<core::State, std::string> StateStore::read_signed(const std::filesystem::path& file) {
    auto text = read_file(file);
    if (!text) return std::unexpected(text.error());

    // header \n hmac \n json
    const std::size_t first = text->find('\n');
    const std::size_t second = first == std::string::npos ? std::string::npos : text->find('\n', first + 1);
    if (second == std::string::npos || std::string_view(*text).substr(0, first) != kHeader) {
        return std::unexpected("not a Sirenproof state file");
    }
    const std::string_view signature = std::string_view(*text).substr(first + 1, second - first - 1);
    const std::string_view json = std::string_view(*text).substr(second + 1);

    auto k = key();
    if (!k) return std::unexpected(k.error());
    const auto expected = core::hmac_sha256(bytes(*k), json);
    const auto given = core::from_hex(signature);
    if (!given || !core::constant_time_equal(expected, *given)) return std::unexpected("the signature doesn't match");

    auto state = core::parse_state(json);
    if (!state) return std::unexpected(state.error());
    return *std::move(state);
}

std::expected<StateStore::Loaded, std::string> StateStore::load() {
    const std::filesystem::path main = dir_ / L"state.dat";
    const std::filesystem::path backup = dir_ / L"state.dat.bak";
    if (!std::filesystem::exists(main) && !std::filesystem::exists(backup)) {
        if (auto k = key(); !k) return std::unexpected(k.error());
        return Loaded{core::State{}, true, {}};
    }

    auto state = read_signed(main);
    if (state) return Loaded{*std::move(state), false, {}};

    auto previous = read_signed(backup);
    if (previous) {
        return Loaded{*std::move(previous), false, "state.dat was unusable (" + state.error() + "); used the backup"};
    }
    return std::unexpected(std::format("state.dat: {}; state.dat.bak: {}", state.error(), previous.error()));
}

std::expected<void, std::string> StateStore::save(const core::State& state) {
    auto k = key();
    if (!k) return std::unexpected(k.error());

    const std::string json = core::serialise_state(state);
    const std::string signature = core::to_hex(core::hmac_sha256(bytes(*k), json));
    const std::string content = std::string(kHeader) + "\n" + signature + "\n" + json;

    const std::filesystem::path main = dir_ / L"state.dat";
    if (std::filesystem::exists(main)) {
        if (auto current = read_file(main); current && *current != content) {
            if (auto kept = write_file_atomic(dir_ / L"state.dat.bak", *current); !kept) return kept;
        }
    }
    return write_file_atomic(main, content);
}

}  // namespace sirenproof::platform
