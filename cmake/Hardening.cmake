# sirenproof_harden(<target>)
# Exploit mitigations for everything we ship. Sirenproof runs a SYSTEM service, so a memory bug in it
# is a privilege-escalation bug; these flags make such bugs harder to exploit.
function(sirenproof_harden target)
    target_compile_options(${target} PRIVATE -fstack-protector-strong)
    # _FORTIFY_SOURCE needs optimisation to do anything; skip it in unoptimised debug builds.
    target_compile_definitions(${target} PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)

    get_target_property(type ${target} TYPE)
    if(type STREQUAL "EXECUTABLE" OR type STREQUAL "SHARED_LIBRARY")
        target_link_options(${target} PRIVATE
            -fstack-protector-strong
            -Wl,--dynamicbase        # ASLR
            -Wl,--high-entropy-va    # 64-bit ASLR
            -Wl,--nxcompat)          # DEP
    endif()
endfunction()
