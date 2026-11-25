option(ARISA_BUILD_FUZZERS "Build the libFuzzer targets (needs clang)" OFF)

if(ARISA_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ARISA_BUILD_FUZZERS needs clang for -fsanitize=fuzzer")
    endif()
    if(NOT ARISA_PROVIDERS_HEADLESS)
        message(FATAL_ERROR "ARISA_BUILD_FUZZERS reads fixture fonts through the headless providers")
    endif()

    function(arisa_add_fuzzer name)
        cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "LIBS")
        add_executable(arisa_${name}_fuzz "${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/${name}_fuzz.cpp")
        target_link_libraries(arisa_${name}_fuzz PRIVATE ${arg_LIBS})
        target_compile_definitions(arisa_${name}_fuzz PRIVATE
            ARISA_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/assets"
            ARISA_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
        target_compile_options(arisa_${name}_fuzz PRIVATE -fsanitize=fuzzer)
        target_link_options(arisa_${name}_fuzz PRIVATE -fsanitize=fuzzer)
        arisa_enable_strict_warnings(arisa_${name}_fuzz)
    endfunction()

    arisa_add_fuzzer(theme LIBS arisa_appearance)
    arisa_add_fuzzer(image LIBS arisa_provider)
    arisa_add_fuzzer(text LIBS arisa_text arisa_provider_headless arisa_provider_standard)
endif()
