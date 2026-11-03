# sirenproof_platform: everything that talks to Windows. The logic itself lives in sirenproof_core.

add_library(sirenproof_platform STATIC
    src/accounts.cpp
    src/app_blocker.cpp
    src/browsers.cpp
    src/clock.cpp
    src/files.cpp
    src/hosts_file.cpp
    src/log.cpp
    src/native_host.cpp
    src/paths.cpp
    src/pipe.cpp
    src/process.cpp
    src/random.cpp
    src/state_store.cpp
    src/text.cpp)
add_library(Sirenproof::platform ALIAS sirenproof_platform)

target_include_directories(sirenproof_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(sirenproof_platform PUBLIC
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
target_link_libraries(sirenproof_platform
    PUBLIC Sirenproof::core
    PRIVATE advapi32 bcrypt crypt32 netapi32 ole32 shell32 uuid version wintrust)

sirenproof_set_warnings(sirenproof_platform)
sirenproof_harden(sirenproof_platform)
