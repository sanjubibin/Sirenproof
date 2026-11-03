add_executable(sirenproof_service
    src/host.cpp
    src/main.cpp
    src/protocol.cpp
    src/scm.cpp)
set_target_properties(sirenproof_service PROPERTIES OUTPUT_NAME sirenproof-service)
target_link_libraries(sirenproof_service PRIVATE Sirenproof::platform)
target_compile_definitions(sirenproof_service PRIVATE $<$<CONFIG:Debug>:SIRENPROOF_DEBUG_BUILD>)
# Static: a SYSTEM service must never load the C++ runtime DLLs from some folder on the PATH.
target_link_options(sirenproof_service PRIVATE -municode -static)

sirenproof_set_warnings(sirenproof_service)
sirenproof_harden(sirenproof_service)
