add_executable(sirenproof_cli src/main.cpp)
set_target_properties(sirenproof_cli PROPERTIES OUTPUT_NAME sirenproof)
target_link_libraries(sirenproof_cli PRIVATE Sirenproof::platform)
target_link_options(sirenproof_cli PRIVATE -static)

sirenproof_set_warnings(sirenproof_cli)
sirenproof_harden(sirenproof_cli)
