# sirenproof-tray: the always-on part of the interface. Plain Win32, no Qt, so it costs next to nothing.
# It also draws the pause for programs on a Nudge rule, with Direct2D.

add_executable(sirenproof_tray WIN32 src/main.cpp src/pause_overlay.cpp)
set_target_properties(sirenproof_tray PROPERTIES OUTPUT_NAME sirenproof-tray)
target_link_libraries(sirenproof_tray PRIVATE Sirenproof::platform shell32 user32 gdi32 d2d1 dwrite windowscodecs ole32 uuid psapi)
target_link_options(sirenproof_tray PRIVATE -municode -static)

sirenproof_set_warnings(sirenproof_tray)
sirenproof_harden(sirenproof_tray)
