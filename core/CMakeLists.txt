# sirenproof_core: pure C++ logic shared by every Sirenproof program.
# It must stay free of Windows headers so it can be unit-tested without touching the system.

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/sirenproof/core/version.hpp"
    @ONLY)

add_library(sirenproof_core STATIC
    src/domain.cpp)
add_library(Sirenproof::core ALIAS sirenproof_core)

target_include_directories(sirenproof_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include")
target_compile_features(sirenproof_core PUBLIC cxx_std_20)

sirenproof_set_warnings(sirenproof_core)
sirenproof_harden(sirenproof_core)
