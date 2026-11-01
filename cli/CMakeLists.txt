add_executable(sirenproof_cli src/main.cpp)
set_target_properties(sirenproof_cli PROPERTIES OUTPUT_NAME sirenproof)
target_link_libraries(sirenproof_cli PRIVATE Sirenproof::core)

sirenproof_set_warnings(sirenproof_cli)
sirenproof_harden(sirenproof_cli)
