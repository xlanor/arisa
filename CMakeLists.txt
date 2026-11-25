cmake_minimum_required(VERSION 3.28)

project(arisa VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(ARISA_BUILD_TESTS "Build the GoogleTest suites and register them with ctest" ON)
option(ARISA_BUILD_CONFORMANCE_NRO "Build the Switch conformance NRO" OFF)
option(ARISA_DEBUG_INFO "Compile every target with debug info" OFF)
if(ARISA_DEBUG_INFO)
    add_compile_options(-g)
endif()
set(ARISA_FRAME_POINTER_OPTIONS -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer)
set(ARISA_TRAP_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/arisa/provider/diagnostics/trap.h")
set(ARISA_TRAP_OPTION
    "$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<BOOL:$<TARGET_PROPERTY:PRECOMPILE_HEADERS>>>>:SHELL:-include ${ARISA_TRAP_HEADER}>")
if(NINTENDO_SWITCH)
    add_compile_options(${ARISA_FRAME_POINTER_OPTIONS} ${ARISA_TRAP_OPTION})
endif()

include(cmake/arisa_warnings.cmake)

add_library(arisa_support INTERFACE)
target_include_directories(arisa_support INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(arisa_support INTERFACE cxx_std_23)

add_library(arisa_async STATIC
    src/async/frame_pool.cpp
    src/async/frame_timeline.cpp
    src/async/job.cpp
    src/async/manual_executor.cpp
    src/async/task_control.cpp
    src/async/task_promise.cpp)
target_link_libraries(arisa_async PUBLIC arisa_support)
arisa_enable_strict_warnings(arisa_async)

add_library(arisa_stb_image STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/stb_image.cpp")
target_include_directories(arisa_stb_image SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb")
arisa_disable_exceptions_and_rtti(arisa_stb_image)
target_compile_options(arisa_stb_image PRIVATE -fwrapv)
if(NINTENDO_SWITCH)
    target_compile_options(arisa_stb_image PRIVATE -O2)
endif()

add_library(arisa_provider STATIC
    src/provider/assets/asset_blob.cpp
    src/provider/assets/asset_path.cpp
    src/provider/audio/absent.cpp
    src/provider/audio/app_voice.cpp
    src/provider/audio/pcm_sound.cpp
    src/provider/audio/sound_bank.cpp
    src/provider/audio/voice_slots.cpp
    src/provider/applets/absent.cpp
    src/provider/speech/absent.cpp
    src/provider/browser/absent.cpp
    src/provider/browser/url.cpp
    src/provider/clipboard/absent.cpp
    src/provider/clipboard/provider.cpp
    src/provider/counted_scope.cpp
    src/provider/diagnostics/absent.cpp
    src/provider/diagnostics/backtrace.cpp
    src/provider/diagnostics/crash_report.cpp
    src/provider/diagnostics/crash_reporter.cpp
    src/provider/diagnostics/heap_meter.cpp
    src/provider/diagnostics/loaded_modules.cpp
    src/provider/diagnostics/memory_space.cpp
    src/provider/diagnostics/thread_cpu_meter.cpp
    src/provider/error.cpp
    src/provider/factory.cpp
    src/provider/fonts/absent.cpp
    src/provider/fonts/font_bank.cpp
    src/provider/gpu/absent.cpp
    src/provider/gpu/frame_hook.cpp
    src/provider/gpu/frame_packet.cpp
    src/provider/gpu/frame_schedule.cpp
    src/provider/gpu/frame_slots.cpp
    src/provider/gpu/frame_source.cpp
    src/provider/gpu/frame_planner.cpp
    src/provider/gpu/free_ranges.cpp
    src/provider/gpu/render_queue.cpp
    src/provider/gpu/gpu_frame.cpp
    src/provider/gpu/gpu_surface.cpp
    src/provider/gpu/gpu_takeover.cpp
    src/provider/gpu/image.cpp
    src/provider/gpu/overlay_slots.cpp
    src/provider/gpu/surface_presence.cpp
    src/provider/gpu/texture.cpp
    src/provider/gpu/texture_publications.cpp
    src/provider/haptics/absent.cpp
    src/provider/haptics/rumble_queue.cpp
    src/provider/haptics/sensory_feedback.cpp
    src/provider/input/absent.cpp
    src/provider/input/input_tuning.cpp
    src/provider/input/key_repeat.cpp
    src/provider/input/touch_panel.cpp
    src/provider/lifecycle/absent.cpp
    src/provider/lifecycle/home_button_block.cpp
    src/provider/surface/absent.cpp
    src/provider/surface/surface_scale.cpp
    src/provider/system_info/absent.cpp
    src/provider/system_info/dimming_inhibit.cpp
    src/provider/text_input/absent.cpp
    src/provider/text_input/text_input_session.cpp
    src/provider/text_input/text_input_traits.cpp
    src/provider/threading/thread.cpp
    src/provider/threading/thread_placement.cpp
    src/provider/threading/thread_placement_policy.cpp)
target_link_libraries(arisa_provider PUBLIC arisa_support PRIVATE arisa_stb_image)
arisa_enable_strict_warnings(arisa_provider)
set(ARISA_CRASH_LINK_OPTIONS
    -Wl,--undefined=__libnx_exception_entry
    -Wl,--wrap=abort
    -Wl,--wrap=__assert_func
    -Wl,--wrap=diagAbortWithResult
    -Wl,--wrap=fatalThrow
    -Wl,--wrap=fatalThrowWithPolicy)
if(NINTENDO_SWITCH)
    target_link_options(arisa_provider INTERFACE ${ARISA_CRASH_LINK_OPTIONS})
    target_compile_options(arisa_provider INTERFACE
        "$<BUILD_INTERFACE:${ARISA_TRAP_OPTION}>" ${ARISA_FRAME_POINTER_OPTIONS})
endif()

add_library(arisa_allocation_meter STATIC
    src/provider/diagnostics/allocation_meter.cpp)
target_link_libraries(arisa_allocation_meter PUBLIC arisa_support)
arisa_enable_strict_warnings(arisa_allocation_meter)

add_library(arisa_async_threads STATIC src/async/thread_pool.cpp)
target_link_libraries(arisa_async_threads PUBLIC arisa_async arisa_provider)
arisa_enable_strict_warnings(arisa_async_threads)

include(cmake/arisa_effects.cmake)
include(cmake/providers/standard.cmake)
include(cmake/providers/headless.cmake)
include(cmake/providers/vulkan.cmake)
include(cmake/providers/horizon.cmake)
include(cmake/providers/sdl3.cmake)
include(cmake/arisa_assets.cmake)
include(cmake/arisa_text.cmake)
include(cmake/arisa_ui.cmake)
include(cmake/arisa_inspector.cmake)
include(cmake/arisa_shaders.cmake)
include(cmake/arisa_examples.cmake)

add_library(arisa_conformance STATIC
    src/conformance/assets_suite.cpp
    src/conformance/audio_suite.cpp
    src/conformance/browser_suite.cpp
    src/conformance/clipboard_suite.cpp
    src/conformance/clock_suite.cpp
    src/conformance/conformance_suite.cpp
    src/conformance/diagnostics_suite.cpp
    src/conformance/fonts_suite.cpp
    src/conformance/gpu_suite.cpp
    src/conformance/haptics_suite.cpp
    src/conformance/input_suite.cpp
    src/conformance/lifecycle_suite.cpp
    src/conformance/report.cpp
    src/conformance/surface_suite.cpp
    src/conformance/system_info_suite.cpp
    src/conformance/text_input_suite.cpp
    src/conformance/threading_suite.cpp)
target_link_libraries(arisa_conformance PUBLIC arisa_provider)
arisa_enable_strict_warnings(arisa_conformance)

include(cmake/arisa_package.cmake)

if(ARISA_BUILD_TESTS)
    enable_testing()
    include(cmake/arisa_tests.cmake)
endif()

include(cmake/arisa_fuzz.cmake)

if(ARISA_BUILD_CONFORMANCE_NRO)
    if(NOT NINTENDO_SWITCH)
        message(FATAL_ERROR "ARISA_BUILD_CONFORMANCE_NRO needs the devkitPro Switch toolchain")
    endif()
    if(NOT ARISA_PROVIDERS_HORIZON)
        message(FATAL_ERROR "ARISA_BUILD_CONFORMANCE_NRO runs HorizonProviderFactory and needs ARISA_PROVIDERS_HORIZON")
    endif()
    add_executable(arisa_conformance_nro switch/conformance_main.cpp)
    target_link_libraries(arisa_conformance_nro PRIVATE
        arisa_conformance arisa_provider_horizon arisa_switch_log)
    arisa_enable_strict_warnings(arisa_conformance_nro)
    nx_generate_nacp(arisa_conformance.nacp
        NAME "arisa conformance${ARISA_NACP_SUFFIX}"
        AUTHOR "xlanor"
        VERSION "${PROJECT_VERSION}")
    nx_create_nro(arisa_conformance_nro
        NACP arisa_conformance.nacp
        ROMFS "${ARISA_ROMFS_DIR}")
endif()
