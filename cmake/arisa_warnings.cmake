option(ARISA_UNITY_BUILD "Compile arisa's own targets as unity batches" OFF)
set(ARISA_UNITY_BATCH_SIZE 8 CACHE STRING "Sources per unity batch for arisa's own targets")
set(ARISA_STACK_USAGE_LIMIT 16384 CACHE STRING "Largest stack frame, in bytes, any function in arisa's own targets may have")
option(ARISA_PRECOMPILED_HEADERS "Precompile the standard headers for arisa's own targets" ON)
set(ARISA_PRECOMPILED_STD_HEADERS
    <algorithm> <array> <chrono> <cstddef> <cstdint> <expected> <format>
    <functional> <map> <memory> <optional> <span> <string> <string_view>
    <utility> <variant> <vector>)

function(arisa_enable_strict_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wsuggest-override
        -Wshadow
        $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast -Wmismatched-new-delete -Wdangling-reference>)
    if(NOT CMAKE_CXX_FLAGS MATCHES "-fsanitize")
        target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:GNU>:-Wstack-usage=${ARISA_STACK_USAGE_LIMIT}>)
    endif()
    arisa_disable_exceptions_and_rtti(${target})
    set_target_properties(${target} PROPERTIES
        UNITY_BUILD ${ARISA_UNITY_BUILD}
        UNITY_BUILD_BATCH_SIZE ${ARISA_UNITY_BATCH_SIZE})
    get_target_property(type ${target} TYPE)
    if(ARISA_PRECOMPILED_HEADERS AND NOT type STREQUAL "INTERFACE_LIBRARY")
        if(NINTENDO_SWITCH)
            target_precompile_headers(${target} PRIVATE "${ARISA_TRAP_HEADER}")
        endif()
        target_precompile_headers(${target} PRIVATE ${ARISA_PRECOMPILED_STD_HEADERS})
    endif()
endfunction()

function(arisa_disable_exceptions_and_rtti target)
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
endfunction()
