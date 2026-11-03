add_executable(sirenproof_host src/main.cpp)
set_target_properties(sirenproof_host PROPERTIES OUTPUT_NAME sirenproof-host)
target_link_libraries(sirenproof_host PRIVATE Sirenproof::platform)
target_link_options(sirenproof_host PRIVATE -municode -static)

sirenproof_set_warnings(sirenproof_host)
sirenproof_harden(sirenproof_host)
