set(arisa_providers_sdl3_default OFF)
if(NOT NINTENDO_SWITCH)
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        set(arisa_providers_sdl3_default ON)
    endif()
endif()

option(ARISA_PROVIDERS_SDL3
    "Build the SDL3 desktop providers and Sdl3ProviderFactory"
    ${arisa_providers_sdl3_default})

if(ARISA_PROVIDERS_SDL3)
    if(NINTENDO_SWITCH)
        message(FATAL_ERROR "ARISA_PROVIDERS_SDL3 is a desktop target and cannot build under the Switch toolchain")
    endif()
    find_package(SDL3 CONFIG QUIET)
    if(NOT SDL3_FOUND)
        message(FATAL_ERROR "ARISA_PROVIDERS_SDL3 is ON but SDL3 was not found; install SDL3 or set ARISA_PROVIDERS_SDL3=OFF")
    endif()
    set_property(TARGET SDL3::Headers PROPERTY SYSTEM ON)
    if(NOT ARISA_PROVIDERS_STANDARD)
        message(FATAL_ERROR
            "ARISA_PROVIDERS_SDL3 uses the standard threading, clock, diagnostics and assets providers and needs ARISA_PROVIDERS_STANDARD")
    endif()
    find_program(ARISA_GLSLANG NAMES glslangValidator glslang REQUIRED)
    set(arisa_spirv_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/spirv")
    set(arisa_spirv_binaries)
    foreach(program shape textured)
        foreach(stage vert frag)
            set(arisa_spirv_source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${program}.${stage}")
            if(program STREQUAL "shape" AND stage STREQUAL "frag")
                set(arisa_spirv_source "${ARISA_SHAPE_FRAGMENT}")
            endif()
            add_custom_command(
                OUTPUT "${arisa_spirv_generated}/${program}_${stage}.spv"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${arisa_spirv_generated}"
                COMMAND "${ARISA_GLSLANG}" -V --target-env vulkan1.0 -S ${stage}
                    -o "${arisa_spirv_generated}/${program}_${stage}.spv"
                    "${arisa_spirv_source}"
                DEPENDS "${arisa_spirv_source}"
                VERBATIM)
            list(APPEND arisa_spirv_binaries "${arisa_spirv_generated}/${program}_${stage}.spv")
        endforeach()
    endforeach()
    add_custom_command(
        OUTPUT "${arisa_spirv_generated}/sdl3_shaders.cpp"
        COMMAND "${CMAKE_COMMAND}"
            "-DHEADER=provider/sdl3/sdl3_shaders.hpp"
            "-DNAMESPACE=arisa::provider::sdl3"
            "-DCLASS=Sdl3Shaders"
            "-DSHADERS=textured_vertex=${arisa_spirv_generated}/textured_vert.spv|textured_fragment=${arisa_spirv_generated}/textured_frag.spv|shape_vertex=${arisa_spirv_generated}/shape_vert.spv|shape_fragment=${arisa_spirv_generated}/shape_frag.spv"
            "-DOUTPUT=${arisa_spirv_generated}/sdl3_shaders.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts/embed_shaders.cmake"
        DEPENDS
            ${arisa_spirv_binaries}
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts/embed_shaders.cmake"
        VERBATIM)

    add_library(arisa_provider_sdl3 STATIC
        src/provider/sdl3/sdl3_audio.cpp
        src/provider/sdl3/sdl3_browser.cpp
        src/provider/sdl3/sdl3_clipboard.cpp
        src/provider/sdl3/sdl3_context.cpp
        src/provider/sdl3/sdl3_event_pump.cpp
        src/provider/sdl3/sdl3_factory.cpp
        src/provider/sdl3/sdl3_fonts.cpp
        src/provider/sdl3/sdl3_gpu.cpp
        src/provider/sdl3/sdl3_haptics.cpp
        src/provider/sdl3/sdl3_input.cpp
        src/provider/sdl3/sdl3_lifecycle.cpp
        src/provider/sdl3/sdl3_offscreen_surface.cpp
        src/provider/sdl3/sdl3_system_info.cpp
        src/provider/sdl3/sdl3_text_input.cpp
        src/provider/sdl3/sdl3_window_surface.cpp
        "${arisa_spirv_generated}/sdl3_shaders.cpp")
    target_include_directories(arisa_provider_sdl3 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(arisa_provider_sdl3
        PUBLIC arisa_provider arisa_provider_standard SDL3::SDL3)
    if(APPLE)
        find_library(ARISA_VULKAN_LOADER NAMES vulkan.1 vulkan)
    endif()
    arisa_enable_strict_warnings(arisa_provider_sdl3)
endif()
