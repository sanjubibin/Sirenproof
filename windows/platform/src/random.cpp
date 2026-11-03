#include "sirenproof/platform/win.hpp"

#include <bcrypt.h>

#include <cstdlib>

#include "sirenproof/platform/system.hpp"

namespace sirenproof::platform {

void SystemRandom::fill(std::span<std::uint8_t> out) {
    // BCryptGenRandom only fails on invalid arguments. Never hand out predictable bytes instead.
    if (BCryptGenRandom(nullptr, out.data(), static_cast<ULONG>(out.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        std::abort();
    }
}

}  // namespace sirenproof::platform
