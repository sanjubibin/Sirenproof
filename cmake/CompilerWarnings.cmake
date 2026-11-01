# sirenproof_set_warnings(<target>)
# Strict warnings for our own code. Third-party headers come in as SYSTEM includes and stay quiet.
function(sirenproof_set_warnings target)
    target_compile_options(${target} PRIVATE
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
    if(SIRENPROOF_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()
