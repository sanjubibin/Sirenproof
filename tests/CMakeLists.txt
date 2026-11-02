find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(core_tests
    core/app_tests.cpp
    core/clock_tests.cpp
    core/crypto_tests.cpp
    core/domain_tests.cpp
    core/hosts_tests.cpp
    core/policy_tests.cpp
    core/rule_tests.cpp
    core/schedule_tests.cpp
    core/state_json_tests.cpp
    core/target_tests.cpp
    core/time_tests.cpp
    core/url_tests.cpp
    core/version_tests.cpp)
target_link_libraries(core_tests PRIVATE Sirenproof::core Catch2::Catch2WithMain)
target_compile_definitions(core_tests PRIVATE SIRENPROOF_EXAMPLES_DIR="${PROJECT_SOURCE_DIR}/examples")
sirenproof_set_warnings(core_tests)

# PRE_TEST: list the test cases when ctest runs, not at build time, so a build never has to run tests.
catch_discover_tests(core_tests DISCOVERY_MODE PRE_TEST)

# Windows layer. Uses temporary files, private pipe names and the sleeper below; never the real
# hosts file, services or other programs.
add_executable(sleeper platform/sleeper.cpp)

add_executable(platform_tests platform/platform_tests.cpp)
target_link_libraries(platform_tests PRIVATE Sirenproof::platform Catch2::Catch2WithMain)
target_compile_definitions(platform_tests PRIVATE SIRENPROOF_SLEEPER_EXE="$<TARGET_FILE:sleeper>")
add_dependencies(platform_tests sleeper)
sirenproof_set_warnings(platform_tests)
catch_discover_tests(platform_tests DISCOVERY_MODE PRE_TEST)
