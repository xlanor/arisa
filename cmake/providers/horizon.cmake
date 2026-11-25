if(NINTENDO_SWITCH)
    set(arisa_providers_horizon_default ON)
else()
    set(arisa_providers_horizon_default OFF)
endif()

option(ARISA_PROVIDERS_HORIZON
    "Build the Horizon (libnx) providers and HorizonProviderFactory"
    ${arisa_providers_horizon_default})

if(ARISA_PROVIDERS_HORIZON AND NOT NINTENDO_SWITCH)
    message(FATAL_ERROR "ARISA_PROVIDERS_HORIZON needs the devkitPro Switch toolchain")
endif()

if(ARISA_PROVIDERS_HORIZON AND NOT ARISA_PROVIDERS_STANDARD)
    message(FATAL_ERROR
        "ARISA_PROVIDERS_HORIZON uses DirectoryAssets and StderrDiagnostics and needs ARISA_PROVIDERS_STANDARD")
endif()

if(NINTENDO_SWITCH2)
    set(ARISA_HORIZON_CONSOLE ounce)
    set(ARISA_HORIZON_GPU vulkan CACHE STRING
        "GPU API on the Switch 2: vulkan (OunceVK), drako3d, or both (chosen at start-up through GpuOptions::api)")
    set_property(CACHE ARISA_HORIZON_GPU PROPERTY STRINGS vulkan drako3d both)
    if(NOT ARISA_HORIZON_GPU MATCHES "^(vulkan|drako3d|both)$")
        message(FATAL_ERROR "ARISA_HORIZON_GPU must be vulkan, drako3d or both on the Switch 2, got ${ARISA_HORIZON_GPU}")
    endif()
else()
    set(ARISA_HORIZON_CONSOLE nx)
    set(ARISA_HORIZON_GPU deko3d)
endif()

set(ARISA_HORIZON_DEKO3D OFF)
if(NINTENDO_SWITCH AND NOT ARISA_HORIZON_GPU STREQUAL "vulkan")
    set(ARISA_HORIZON_DEKO3D ON)
endif()
set(ARISA_HORIZON_VULKAN OFF)
if(NINTENDO_SWITCH2 AND ARISA_HORIZON_GPU MATCHES "^(vulkan|both)$")
    set(ARISA_HORIZON_VULKAN ON)
endif()

if(ARISA_HORIZON_GPU MATCHES "^(drako3d|both)$")
    find_program(ARISA_UAM2_EXE uam2 REQUIRED)
    set(ARISA_DKSH_COMPILER "${ARISA_UAM2_EXE}")
    set(ARISA_DEKO3D_LIBRARY drako3d)
    if(ARISA_HORIZON_GPU STREQUAL "drako3d")
        set(ARISA_NACP_SUFFIX " drako3d")
    endif()
    set(ARISA_DEKO3D_SHIM "${CMAKE_CURRENT_BINARY_DIR}/generated/drako3d_shim")
    file(CONFIGURE OUTPUT "${ARISA_DEKO3D_SHIM}/deko3d.h"
        CONTENT "#pragma once\n\n#include <drako3d.h>\n" @ONLY)
    file(CONFIGURE OUTPUT "${ARISA_DEKO3D_SHIM}/deko3d.hpp"
        CONTENT "#pragma once\n\n#include <drako3d.hpp>\n" @ONLY)
elseif(ARISA_HORIZON_DEKO3D)
    set(ARISA_DKSH_COMPILER "${NX_UAM_EXE}")
    set(ARISA_DEKO3D_LIBRARY deko3d)
endif()

if(ARISA_PROVIDERS_HORIZON AND ARISA_HORIZON_DEKO3D)
    if(NOT ARISA_DKSH_COMPILER)
        message(FATAL_ERROR "ARISA_PROVIDERS_HORIZON needs uam (or uam2 for drako3d) to compile the deko3d shaders")
    endif()
    set(arisa_deko3d_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/deko3d")
    set(arisa_deko3d_sources
        "textured_vsh=vert=${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured.vert"
        "textured_fsh=frag=${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured.frag"
        "shape_vsh=vert=${CMAKE_CURRENT_SOURCE_DIR}/shaders/shape.vert"
        "shape_fsh=frag=${ARISA_SHAPE_FRAGMENT}")
    set(arisa_deko3d_binaries)
    foreach(entry IN LISTS arisa_deko3d_sources)
        string(REPLACE "=" ";" parts "${entry}")
        list(GET parts 0 name)
        list(GET parts 1 stage)
        list(GET parts 2 source)
        add_custom_command(
            OUTPUT "${arisa_deko3d_generated}/${name}.dksh"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${arisa_deko3d_generated}"
            COMMAND "${ARISA_DKSH_COMPILER}" -o "${arisa_deko3d_generated}/${name}.dksh"
                -s ${stage} "${source}"
            DEPENDS "${source}"
            VERBATIM)
        list(APPEND arisa_deko3d_binaries "${arisa_deko3d_generated}/${name}.dksh")
    endforeach()
    add_custom_command(
        OUTPUT "${arisa_deko3d_generated}/deko3d_shaders.cpp"
        COMMAND "${CMAKE_COMMAND}"
            "-DHEADER=provider/horizon/deko3d_shaders.hpp"
            "-DNAMESPACE=arisa::provider::horizon"
            "-DCLASS=Deko3dShaders"
            "-DSHADERS=textured_vertex=${arisa_deko3d_generated}/textured_vsh.dksh|textured_fragment=${arisa_deko3d_generated}/textured_fsh.dksh|shape_vertex=${arisa_deko3d_generated}/shape_vsh.dksh|shape_fragment=${arisa_deko3d_generated}/shape_fsh.dksh"
            "-DOUTPUT=${arisa_deko3d_generated}/deko3d_shaders.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts/embed_shaders.cmake"
        DEPENDS
            ${arisa_deko3d_binaries}
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts/embed_shaders.cmake"
        VERBATIM)
    set(arisa_horizon_console_sources
        src/provider/horizon/deko3d_gpu.cpp
        src/provider/horizon/deko3d_memory.cpp
        "${arisa_deko3d_generated}/deko3d_shaders.cpp")
    if(ARISA_HORIZON_CONSOLE STREQUAL "nx")
        list(APPEND arisa_horizon_console_sources
            src/provider/horizon/nx/nx_console.cpp
            src/provider/horizon/nx/nx_pointer.cpp)
    else()
        list(APPEND arisa_horizon_console_sources
            src/provider/horizon/ounce/ounce_hardware.cpp
            src/provider/horizon/ounce/drako_console.cpp
            src/provider/horizon/ounce/ounce_pointer.cpp)
    endif()
    set(arisa_horizon_console_libraries ${ARISA_DEKO3D_LIBRARY})
    if(ARISA_HORIZON_VULKAN)
        if(NOT ARISA_PROVIDERS_VULKAN)
            message(FATAL_ERROR "ARISA_HORIZON_GPU=both draws through ARISA_PROVIDERS_VULKAN as well")
        endif()
        list(APPEND arisa_horizon_console_sources
            src/provider/horizon/ounce/ounce_console.cpp)
        list(APPEND arisa_horizon_console_libraries arisa_provider_vulkan)
    endif()
elseif(ARISA_PROVIDERS_HORIZON)
    if(NOT ARISA_PROVIDERS_VULKAN)
        message(FATAL_ERROR "ARISA_PROVIDERS_HORIZON on ounce with ARISA_HORIZON_GPU=vulkan draws through ARISA_PROVIDERS_VULKAN")
    endif()
    set(arisa_horizon_console_sources
        src/provider/horizon/ounce/ounce_hardware.cpp
        src/provider/horizon/ounce/ounce_console.cpp
        src/provider/horizon/ounce/ounce_pointer.cpp)
    set(arisa_horizon_console_libraries arisa_provider_vulkan)
endif()

if(ARISA_PROVIDERS_HORIZON)
    add_library(arisa_provider_horizon STATIC
        src/provider/horizon/horizon_audio.cpp
        src/provider/horizon/horizon_applets.cpp
        src/provider/horizon/horizon_browser.cpp
        src/provider/horizon/horizon_clock.cpp
        src/provider/horizon/horizon_factory.cpp
        src/provider/horizon/horizon_fonts.cpp
        src/provider/horizon/horizon_haptics.cpp
        src/provider/horizon/horizon_input.cpp
        src/provider/horizon/horizon_lifecycle.cpp
        src/provider/horizon/horizon_surface.cpp
        src/provider/horizon/horizon_system_info.cpp
        src/provider/horizon/horizon_text_input.cpp
        src/provider/horizon/horizon_threading.cpp
        ${arisa_horizon_console_sources})
    target_include_directories(arisa_provider_horizon PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(arisa_provider_horizon
        PUBLIC arisa_provider arisa_provider_standard ${arisa_horizon_console_libraries})
    if(ARISA_HORIZON_DEKO3D)
        target_compile_definitions(arisa_provider_horizon PUBLIC ARISA_HORIZON_DEKO3D=1)
    endif()
    if(ARISA_HORIZON_VULKAN)
        target_compile_definitions(arisa_provider_horizon PUBLIC ARISA_HORIZON_VULKAN=1)
    endif()
    if(ARISA_DEKO3D_SHIM)
        target_include_directories(arisa_provider_horizon PUBLIC
            "$<BUILD_INTERFACE:${ARISA_DEKO3D_SHIM}>")
    endif()
    arisa_enable_strict_warnings(arisa_provider_horizon)
endif()

if(ARISA_PROVIDERS_HORIZON)
    add_library(arisa_switch_log STATIC
        switch/exit_trail.cpp
        switch/frame_report.cpp
        switch/nxlink_log.cpp
        switch/ui_thread.cpp)
    target_include_directories(arisa_switch_log PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/switch")
    target_link_libraries(arisa_switch_log PUBLIC arisa_support arisa_provider_horizon arisa_runtime)
    arisa_enable_strict_warnings(arisa_switch_log)
endif()
