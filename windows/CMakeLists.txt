cmake_minimum_required(VERSION 3.25...4.4)
project(Sirenproof
    VERSION 0.1.0
    DESCRIPTION "Open-source, local-only website and app blocker for Windows"
    HOMEPAGE_URL "https://github.com/sanjubibin/Sirenproof"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)  # no C++20 modules; skip the extra scan step

option(SIRENPROOF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SIRENPROOF_BUILD_TESTS "Build the unit tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Hardening)

# All programs side by side: the CLI only trusts a service that runs from its own folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(service)
add_subdirectory(host)
add_subdirectory(tray)
add_subdirectory(cli)

# The main window needs Qt 6 (MSYS2: qt6-base, qt6-declarative); everything else builds without it.
find_package(Qt6 6.8 QUIET COMPONENTS Quick QuickControls2)
if(Qt6_FOUND)
    add_subdirectory(ui)
else()
    message(STATUS "Qt 6 not found: skipping the main window (ui/)")
endif()

if(SIRENPROOF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
