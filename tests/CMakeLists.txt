find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(core_tests
    core/domain_tests.cpp
    core/version_tests.cpp)
target_link_libraries(core_tests PRIVATE Sirenproof::core Catch2::Catch2WithMain)
sirenproof_set_warnings(core_tests)

# PRE_TEST: list the test cases when ctest runs, not at build time, so a build never has to run tests.
catch_discover_tests(core_tests DISCOVERY_MODE PRE_TEST)
