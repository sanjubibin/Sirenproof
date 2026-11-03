#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "sirenproof/core/policy.hpp"

namespace sirenproof::platform {

// Saves and loads the service's State, and notices if anyone edited it.
//
// state.dat holds a header line, an HMAC-SHA-256 of the JSON, and the JSON. The HMAC key lives in
// state.key, encrypted with DPAPI for the account running the service, so only that account (SYSTEM,
// for the installed service) can sign a state. The previous good state is kept as state.dat.bak.
class StateStore {
public:
    explicit StateStore(std::filesystem::path dir);

    struct Loaded {
        core::State state;
        bool fresh = false;      // nothing saved yet
        std::string warning;     // set if the main file was damaged or edited and the backup was used
    };
    // A missing state is a fresh start. A state whose signature doesn't match is never used.
    std::expected<Loaded, std::string> load();
    std::expected<void, std::string> save(const core::State& state);

    const std::filesystem::path& dir() const { return dir_; }

private:
    std::expected<std::string, std::string> key();
    std::expected<core::State, std::string> read_signed(const std::filesystem::path& file);

    std::filesystem::path dir_;
    std::string key_;
};

}  // namespace sirenproof::platform
