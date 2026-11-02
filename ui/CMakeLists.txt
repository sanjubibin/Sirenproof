# sirenproof-ui: the main window. Qt Quick, started from the tray only while it's open.

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

set(SIRENPROOF_UI_SOURCES
    src/json_variant.cpp
    src/main.cpp
    src/service_client.cpp)

qt_add_executable(sirenproof_ui WIN32 ${SIRENPROOF_UI_SOURCES} src/service_client.hpp src/json_variant.hpp)
set_target_properties(sirenproof_ui PROPERTIES OUTPUT_NAME sirenproof-ui)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(sirenproof_ui
    URI Sirenproof
    VERSION 1.0
    QML_FILES
        qml/Card.qml
        qml/CommandPalette.qml
        qml/LockScreen.qml
        qml/Main.qml
        qml/MastRing.qml
        qml/NowScreen.qml
        qml/RulesScreen.qml
        qml/ScheduleGrid.qml
        qml/SirenButton.qml
        qml/Theme.qml)

target_link_libraries(sirenproof_ui PRIVATE Qt6::Quick Qt6::QuickControls2 Sirenproof::platform)

sirenproof_set_source_warnings(${SIRENPROOF_UI_SOURCES})
sirenproof_harden(sirenproof_ui)
