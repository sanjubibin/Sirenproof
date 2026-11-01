cmake_minimum_required(VERSION 3.25...4.4)
project(Sirenproof
    VERSION 0.1.0
    DESCRIPTION "Open-source, local-only website and app blocker for Windows"
    HOMEPAGE_URL "https://github.com/sanjubibin/Sirenproof"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)  # no C++20 modules; skip the extra scan step

option(SIRENPROOF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SIRENPROOF_BUILD_TESTS "Build the unit tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Hardening)

add_subdirectory(core)

if(SIRENPROOF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
