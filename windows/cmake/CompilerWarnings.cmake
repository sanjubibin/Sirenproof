set(SIRENPROOF_WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wold-style-cast
    -Wcast-align
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    -Wuseless-cast
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op)

# sirenproof_set_warnings(<target>)
# Strict warnings for our own code. Third-party headers come in as SYSTEM includes and stay quiet.
function(sirenproof_set_warnings target)
    target_compile_options(${target} PRIVATE ${SIRENPROOF_WARNING_FLAGS})
    if(SIRENPROOF_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

# sirenproof_set_source_warnings(<source>...)
# The same, for chosen source files only: Qt targets also compile moc and QML-cache code we don't
# write, which isn't held to these rules.
function(sirenproof_set_source_warnings)
    set(flags ${SIRENPROOF_WARNING_FLAGS})
    if(SIRENPROOF_WARNINGS_AS_ERRORS)
        list(APPEND flags -Werror)
    endif()
    set_source_files_properties(${ARGN} DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTIES COMPILE_OPTIONS "${flags}")
endfunction()
