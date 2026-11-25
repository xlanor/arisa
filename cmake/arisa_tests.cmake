find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(arisa_gtest_dir "${CMAKE_CURRENT_SOURCE_DIR}/tests/lib/googletest")

add_library(arisa_gtest STATIC "${arisa_gtest_dir}/src/gtest-all.cc")
target_include_directories(arisa_gtest SYSTEM PUBLIC "${arisa_gtest_dir}/include")
target_include_directories(arisa_gtest PRIVATE "${arisa_gtest_dir}")
target_compile_definitions(arisa_gtest PUBLIC GTEST_HAS_EXCEPTIONS=0 GTEST_HAS_RTTI=0)
target_link_libraries(arisa_gtest PUBLIC Threads::Threads)
arisa_disable_exceptions_and_rtti(arisa_gtest)

add_library(arisa_gtest_main STATIC "${CMAKE_CURRENT_SOURCE_DIR}/tests/main/test_main.cpp")
target_link_libraries(arisa_gtest_main PUBLIC arisa_gtest)
arisa_enable_strict_warnings(arisa_gtest_main)

set(ARISA_TEST_SHARD_SCALE 1 CACHE STRING "Multiplier applied to every suite's ctest shard count")

function(arisa_add_test_suite name folder)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "SHARDS" "LIBS;ENVIRONMENT")
    file(GLOB sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/${folder}/*_tests.cpp")
    add_executable(${name} ${sources})
    target_link_libraries(${name} PRIVATE ${arg_LIBS} arisa_gtest_main)
    target_compile_definitions(${name} PRIVATE
        ARISA_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/assets")
    arisa_enable_strict_warnings(${name})
    if(ARISA_PRECOMPILED_HEADERS)
        target_precompile_headers(${name} PRIVATE <gtest/gtest.h>)
    endif()
    if(NOT arg_SHARDS)
        set(arg_SHARDS 1)
    endif()
    math(EXPR shards "${arg_SHARDS} * ${ARISA_TEST_SHARD_SCALE}")
    math(EXPR last "${shards} - 1")
    foreach(index RANGE ${last})
        set(test_name "${folder}/shard_${index}_of_${shards}")
        add_test(NAME "${test_name}"
            COMMAND ${name} --gtest_brief=1
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set(environment "GTEST_TOTAL_SHARDS=${shards}" "GTEST_SHARD_INDEX=${index}"
            ${arg_ENVIRONMENT})
        set_tests_properties("${test_name}" PROPERTIES
            LABELS "${folder}"
            ENVIRONMENT "${environment}")
    endforeach()
endfunction()

arisa_add_test_suite(arisa_support_tests support LIBS arisa_support)
arisa_add_test_suite(arisa_provider_tests provider SHARDS 2 LIBS arisa_provider)

if(ARISA_PROVIDERS_STANDARD)
    arisa_add_test_suite(arisa_async_tests async SHARDS 3
        LIBS arisa_async arisa_async_threads arisa_provider_standard)
endif()

if(ARISA_PROVIDERS_STANDARD)
    arisa_add_test_suite(arisa_provider_standard_tests provider/standard
        LIBS arisa_provider_standard arisa_conformance arisa_allocation_meter)
endif()

if(ARISA_PROVIDERS_HEADLESS)
    arisa_add_test_suite(arisa_provider_headless_tests provider/headless
        LIBS arisa_provider_headless arisa_conformance)
    arisa_add_test_suite(arisa_conformance_tests conformance
        LIBS arisa_conformance arisa_provider_headless arisa_provider_standard)
endif()

set(arisa_sdl3_test_environment
    "SDL_VIDEODRIVER=offscreen"
    "SDL_VIDEO_DRIVER=offscreen"
    "SDL_AUDIO_DRIVER=dummy")
if(ARISA_VULKAN_LOADER)
    list(APPEND arisa_sdl3_test_environment "SDL_VULKAN_LIBRARY=${ARISA_VULKAN_LOADER}")
endif()

if(ARISA_PROVIDERS_SDL3 AND ARISA_PROVIDERS_HEADLESS)
    arisa_add_test_suite(arisa_provider_sdl3_tests provider/sdl3
        LIBS arisa_provider_sdl3 arisa_provider_headless arisa_conformance
            arisa_runtime arisa_components arisa_provider_standard
        ENVIRONMENT ${arisa_sdl3_test_environment})
    if(ARISA_VULKAN_LOADER)
        target_compile_definitions(arisa_provider_sdl3_tests PRIVATE
            "ARISA_VULKAN_LOADER=\"${ARISA_VULKAN_LOADER}\"")
    endif()
endif()

if(ARISA_PROVIDERS_VULKAN AND ARISA_PROVIDERS_HEADLESS AND TARGET arisa_runtime
        AND TARGET arisa_gallery_screen AND TARGET arisa_sample_vulkan)
    arisa_add_test_suite(arisa_provider_vulkan_tests provider/vulkan SHARDS 3
        LIBS arisa_provider_vulkan arisa_provider_headless arisa_conformance
            arisa_gallery_screen arisa_gpu_stage_vulkan arisa_sample_vulkan
            arisa_runtime arisa_stb_image_write)
    target_compile_definitions(arisa_provider_vulkan_tests PRIVATE
        ARISA_TEST_OUTPUT="${CMAKE_BINARY_DIR}"
        ARISA_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ARISA_GPU_STAGE_MEDIA="${CMAKE_CURRENT_SOURCE_DIR}/examples/gpu_stage/media")
endif()

if(TARGET arisa_sample_sdl)
    arisa_add_test_suite(arisa_examples_tests examples
        LIBS arisa_sample_sdl arisa_plasma_shaders arisa_runtime
        ENVIRONMENT ${arisa_sdl3_test_environment})
    get_target_property(arisa_plasma_format arisa_plasma_shaders ARISA_SHADER_FORMAT)
    target_compile_definitions(arisa_examples_tests PRIVATE
        ARISA_PLASMA_SHADERS="${CMAKE_BINARY_DIR}/generated/arisa_plasma_shaders"
        ARISA_PLASMA_SHADER_FORMAT="${arisa_plasma_format}")
endif()

if(ARISA_PROVIDERS_HEADLESS)
    arisa_add_test_suite(arisa_text_tests text SHARDS 2
        LIBS arisa_text arisa_provider_headless arisa_provider_standard)
    target_compile_definitions(arisa_text_tests PRIVATE
        ARISA_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ARISA_TEXT_GOLDENS="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/text"
        ARISA_SWIFTUI_LOCALIZATION="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/metrics_localization.tsv"
        ARISA_SWIFTUI_FORMATS="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/formats.tsv")
    target_include_directories(arisa_text_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/text")
endif()

arisa_add_test_suite(arisa_theme_tests theme LIBS arisa_components arisa_theme)

if(ARISA_PROVIDERS_HEADLESS)
    arisa_add_test_suite(arisa_appearance_tests appearance
        LIBS arisa_appearance arisa_provider_headless)
    target_compile_definitions(arisa_appearance_tests PRIVATE
        ARISA_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ARISA_THEME_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/themes"
        ARISA_DYNAMIC_TYPE_REFERENCE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/dynamic_type.tsv")
endif()

if(ARISA_PROVIDERS_STANDARD)
    arisa_add_test_suite(arisa_motion_tests motion
        LIBS arisa_motion arisa_provider_standard)
    target_compile_definitions(arisa_motion_tests PRIVATE
        ARISA_SWIFTUI_BLEND="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/metrics_blend.tsv"
        ARISA_SWIFTUI_ANIMATORS="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/metrics_animators.tsv"
        ARISA_SWIFTUI_MOTION_PARITY="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/metrics_motion_parity.tsv")
endif()

if(ARISA_PROVIDERS_HEADLESS)
    foreach(suite layout rendering)
        arisa_add_test_suite(arisa_${suite}_tests ${suite} SHARDS 2
            LIBS arisa_layout arisa_rendering arisa_provider_headless arisa_provider_standard)
        target_compile_definitions(arisa_${suite}_tests PRIVATE
            ARISA_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
        target_include_directories(arisa_${suite}_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/layout")
    endforeach()
    target_include_directories(arisa_rendering_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(arisa_rendering_tests PRIVATE
        ARISA_SWIFTUI_PATHS="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/metrics_paths.tsv")
endif()

arisa_add_test_suite(arisa_composition_tests composition SHARDS 2
    LIBS arisa_components arisa_sml)
target_compile_definitions(arisa_composition_tests PRIVATE
    ARISA_SWIFTUI_FORMATS="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/formats.tsv")
target_include_directories(arisa_composition_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

arisa_add_test_suite(arisa_markup_tests markup LIBS arisa_markup)

arisa_add_test_suite(arisa_interaction_tests interaction
    LIBS arisa_interaction arisa_components)
target_include_directories(arisa_interaction_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(TARGET arisa_runtime AND ARISA_PROVIDERS_HEADLESS)
    arisa_add_test_suite(arisa_runtime_tests runtime SHARDS 8
        LIBS arisa_runtime arisa_components arisa_presentation arisa_markup
            arisa_provider_headless arisa_sml)
    target_include_directories(arisa_runtime_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime")
    target_compile_definitions(arisa_runtime_tests PRIVATE
        ARISA_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ARISA_SWIFTUI_GLASS="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/metrics_glass_ios.tsv")
endif()

if(TARGET arisa_runtime AND TARGET arisa_gallery_screen AND ARISA_PROVIDERS_HEADLESS)
    arisa_add_test_suite(arisa_gallery_tests gallery SHARDS 40
        LIBS arisa_gallery_screen arisa_runtime arisa_provider_headless arisa_sml
            arisa_allocation_meter arisa_stb_image_write)
    target_compile_definitions(arisa_gallery_tests PRIVATE
        ARISA_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ARISA_GALLERY_GOLDENS="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/gallery"
        ARISA_SWIFTUI_REFERENCE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/metrics.tsv")
    target_include_directories(arisa_gallery_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/gallery")
endif()

if(TARGET arisa_inspector AND TARGET arisa_gallery_screen AND ARISA_PROVIDERS_HEADLESS)
    arisa_add_test_suite(arisa_inspector_tests inspector SHARDS 4
        LIBS arisa_inspector arisa_gallery_screen arisa_runtime
            arisa_provider_headless arisa_sml arisa_allocation_meter
            arisa_stb_image_write)
    target_compile_definitions(arisa_inspector_tests PRIVATE
        ARISA_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        ARISA_GALLERY_GOLDENS="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/gallery"
        ARISA_SWIFTUI_REFERENCE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/metrics.tsv")
    target_include_directories(arisa_inspector_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/gallery")
endif()

if(TARGET arisa_bench)
    foreach(scenario idle fling gallery navigation sheet theme launch launch_path
            sidebar_scroll sidebar_fast glass_page materials_page
            covered_page materials_leave token_slider_drag)
        add_test(NAME bench/${scenario}
            COMMAND arisa_bench --scenario ${scenario} --budgets
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(bench/${scenario} PROPERTIES LABELS bench
            RUN_SERIAL TRUE)
    endforeach()
    add_test(NAME bench/glass_scroll_real_time
        COMMAND arisa_bench --real-time --scenario detail_scroll
            --scenario sidebar_scroll --budgets
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(bench/glass_scroll_real_time PROPERTIES LABELS bench
        RUN_SERIAL TRUE)
endif()

if(TARGET arisa_gallery)
    foreach(page typography multilingual cards layout themes counter focus scroll
            navigation presentations controls lists icons colours fidelity
            theming settings)
        foreach(size 1280x720 1920x1080)
            set(capture "${CMAKE_BINARY_DIR}/gallery_${page}_${size}.png")
            add_test(NAME examples/gallery_${page}_${size}
                COMMAND arisa_gallery --offscreen ${size} --frames 3 --page ${page}
                    --capture "${capture}")
            set_tests_properties(examples/gallery_${page}_${size} PROPERTIES
                ENVIRONMENT "${arisa_sdl3_test_environment}"
                FIXTURES_SETUP arisa_gallery_${page}_${size})
            add_test(NAME examples/gallery_${page}_${size}_matches_golden
                COMMAND Python3::Interpreter
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_png.py" "${capture}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/gallery/${page}_${size}.png")
            set_tests_properties(examples/gallery_${page}_${size}_matches_golden PROPERTIES
                FIXTURES_REQUIRED arisa_gallery_${page}_${size})
        endforeach()
    endforeach()
endif()

if(TARGET arisa_gallery)
    foreach(page cards layout focus effects presentations controls lists icons
            colours fidelity theming)
        set(capture "${CMAKE_BINARY_DIR}/gallery_${page}_dark_1280x720.png")
        add_test(NAME examples/gallery_${page}_dark_1280x720
            COMMAND arisa_gallery --offscreen 1280x720 --frames 3 --page ${page}
                --scheme dark --capture "${capture}")
        set_tests_properties(examples/gallery_${page}_dark_1280x720 PROPERTIES
            ENVIRONMENT "${arisa_sdl3_test_environment}"
            FIXTURES_SETUP arisa_gallery_${page}_dark)
        add_test(NAME examples/gallery_${page}_dark_1280x720_matches_golden
            COMMAND Python3::Interpreter
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_png.py" "${capture}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/gallery/${page}_dark_1280x720.png")
        set_tests_properties(examples/gallery_${page}_dark_1280x720_matches_golden PROPERTIES
            FIXTURES_REQUIRED arisa_gallery_${page}_dark)
    endforeach()
endif()

if(TARGET arisa_gallery)
    foreach(moment 0 200 2000)
        set(capture "${CMAKE_BINARY_DIR}/gallery_motion_${moment}ms_1280x720.png")
        add_test(NAME examples/gallery_motion_${moment}ms_1280x720
            COMMAND arisa_gallery --offscreen 1280x720 --page motion
                --motion-ms ${moment} --capture "${capture}")
        set_tests_properties(examples/gallery_motion_${moment}ms_1280x720 PROPERTIES
            ENVIRONMENT "${arisa_sdl3_test_environment}"
            FIXTURES_SETUP arisa_gallery_motion_${moment})
        add_test(NAME examples/gallery_motion_${moment}ms_1280x720_matches_golden
            COMMAND Python3::Interpreter
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_png.py" "${capture}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/gallery/motion_${moment}ms_1280x720.png")
        set_tests_properties(examples/gallery_motion_${moment}ms_1280x720_matches_golden PROPERTIES
            FIXTURES_REQUIRED arisa_gallery_motion_${moment})
    endforeach()
endif()

if(TARGET arisa_gallery)
    foreach(moment 0 150 1500)
        set(capture "${CMAKE_BINARY_DIR}/gallery_effects_${moment}ms_1280x720.png")
        add_test(NAME examples/gallery_effects_${moment}ms_1280x720
            COMMAND arisa_gallery --offscreen 1280x720 --page effects
                --motion-ms ${moment} --capture "${capture}")
        set_tests_properties(examples/gallery_effects_${moment}ms_1280x720 PROPERTIES
            ENVIRONMENT "${arisa_sdl3_test_environment}"
            FIXTURES_SETUP arisa_gallery_effects_${moment})
        add_test(NAME examples/gallery_effects_${moment}ms_1280x720_matches_golden
            COMMAND Python3::Interpreter
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_png.py" "${capture}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/gallery/effects_${moment}ms_1280x720.png")
        set_tests_properties(examples/gallery_effects_${moment}ms_1280x720_matches_golden PROPERTIES
            FIXTURES_REQUIRED arisa_gallery_effects_${moment})
    endforeach()
endif()

if(TARGET arisa_gpu_stage)
    add_test(NAME examples/gpu_stage_offscreen
        COMMAND arisa_gpu_stage --offscreen 1280x720 --frames 12
            --capture "${CMAKE_BINARY_DIR}/gpu_stage.png")
    set_tests_properties(examples/gpu_stage_offscreen PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}"
        FIXTURES_SETUP arisa_gpu_stage_png)
    add_test(NAME examples/gpu_stage_capture_is_not_flat
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_png.py"
            "${CMAKE_BINARY_DIR}/gpu_stage.png" 1280 720)
    set_tests_properties(examples/gpu_stage_capture_is_not_flat PROPERTIES
        FIXTURES_REQUIRED arisa_gpu_stage_png)
    add_test(NAME examples/gpu_stage_paused_stops_presenting
        COMMAND arisa_gpu_stage --offscreen 640x360 --frames 24 --script 6)
    set_tests_properties(examples/gpu_stage_paused_stops_presenting PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}")
    add_test(NAME examples/gpu_stage_video_offscreen
        COMMAND arisa_gpu_stage --offscreen 1280x720 --frames 16 --page video
            --capture "${CMAKE_BINARY_DIR}/gpu_stage_video.png")
    set_tests_properties(examples/gpu_stage_video_offscreen PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}"
        FIXTURES_SETUP arisa_gpu_stage_video_png)
    add_test(NAME examples/gpu_stage_video_capture_is_not_flat
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_png.py"
            "${CMAKE_BINARY_DIR}/gpu_stage_video.png" 1280 720)
    set_tests_properties(examples/gpu_stage_video_capture_is_not_flat PROPERTIES
        FIXTURES_REQUIRED arisa_gpu_stage_video_png)
    add_test(NAME examples/gpu_stage_video_paused_stops_presenting
        COMMAND arisa_gpu_stage --offscreen 640x360 --frames 24 --script 6
            --page video)
    set_tests_properties(examples/gpu_stage_video_paused_stops_presenting PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}")
    add_test(NAME examples/gpu_stage_surface_offscreen
        COMMAND arisa_gpu_stage --offscreen 1280x720 --frames 16 --page surface
            --capture "${CMAKE_BINARY_DIR}/gpu_stage_surface.png")
    set_tests_properties(examples/gpu_stage_surface_offscreen PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}"
        FIXTURES_SETUP arisa_gpu_stage_surface_png)
    add_test(NAME examples/gpu_stage_surface_capture_is_not_flat
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_png.py"
            "${CMAKE_BINARY_DIR}/gpu_stage_surface.png" 1280 720)
    set_tests_properties(examples/gpu_stage_surface_capture_is_not_flat PROPERTIES
        FIXTURES_REQUIRED arisa_gpu_stage_surface_png)
    add_test(NAME examples/gpu_stage_surface_paused_stops_presenting
        COMMAND arisa_gpu_stage --offscreen 640x360 --frames 24 --script 6
            --page surface)
    set_tests_properties(examples/gpu_stage_surface_paused_stops_presenting PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}")
    add_test(NAME examples/gpu_stage_nv12_offscreen
        COMMAND arisa_gpu_stage --offscreen 1280x720 --frames 16 --page nv12
            --capture "${CMAKE_BINARY_DIR}/gpu_stage_nv12.png")
    set_tests_properties(examples/gpu_stage_nv12_offscreen PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}"
        FIXTURES_SETUP arisa_gpu_stage_nv12_png)
    add_test(NAME examples/gpu_stage_nv12_capture_is_not_flat
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_png.py"
            "${CMAKE_BINARY_DIR}/gpu_stage_nv12.png" 1280 720)
    set_tests_properties(examples/gpu_stage_nv12_capture_is_not_flat PROPERTIES
        FIXTURES_REQUIRED arisa_gpu_stage_nv12_png)
    add_test(NAME examples/gpu_stage_exclusive_offscreen
        COMMAND arisa_gpu_stage --offscreen 1280x720 --frames 24 --page exclusive)
    set_tests_properties(examples/gpu_stage_exclusive_offscreen PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}")
    add_test(NAME examples/gpu_stage_base_offscreen
        COMMAND arisa_gpu_stage --offscreen 1280x720 --frames 16 --page base
            --capture "${CMAKE_BINARY_DIR}/gpu_stage_base.png")
    set_tests_properties(examples/gpu_stage_base_offscreen PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}"
        FIXTURES_SETUP arisa_gpu_stage_base_png)
    add_test(NAME examples/gpu_stage_base_capture_is_not_flat
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_png.py"
            "${CMAKE_BINARY_DIR}/gpu_stage_base.png" 1280 720)
    set_tests_properties(examples/gpu_stage_base_capture_is_not_flat PROPERTIES
        FIXTURES_REQUIRED arisa_gpu_stage_base_png)
    add_test(NAME examples/gpu_stage_nv12_paused_stops_presenting
        COMMAND arisa_gpu_stage --offscreen 640x360 --frames 24 --script 6
            --page nv12)
    set_tests_properties(examples/gpu_stage_nv12_paused_stops_presenting PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}")
endif()

if(TARGET arisa_sample)
    add_test(NAME examples/sample_offscreen
        COMMAND arisa_sample --offscreen 1280x720 --frames 30
            --capture "${CMAKE_BINARY_DIR}/sample.png")
    set_tests_properties(examples/sample_offscreen PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}"
        LABELS examples
        FIXTURES_SETUP arisa_sample_png)
    add_test(NAME examples/sample_capture_is_not_flat
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_png.py"
            "${CMAKE_BINARY_DIR}/sample.png" 1280 720)
    set_tests_properties(examples/sample_capture_is_not_flat PROPERTIES
        LABELS examples
        FIXTURES_REQUIRED arisa_sample_png)
endif()

if(TARGET arisa_counter)
    add_test(NAME examples/counter_offscreen_unscripted
        COMMAND arisa_counter --offscreen 1280x720 --frames 4 --script 0
            --capture "${CMAKE_BINARY_DIR}/counter_zero.png")
    set_tests_properties(examples/counter_offscreen_unscripted PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}"
        FIXTURES_SETUP arisa_counter_png)
    add_test(NAME examples/counter_offscreen_scripted
        COMMAND arisa_counter --offscreen 1280x720 --frames 4 --script 3
            --capture "${CMAKE_BINARY_DIR}/counter.png")
    set_tests_properties(examples/counter_offscreen_scripted PROPERTIES
        ENVIRONMENT "${arisa_sdl3_test_environment}"
        FIXTURES_SETUP arisa_counter_png)
    add_test(NAME examples/counter_capture_shows_the_count
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_png.py"
            "${CMAKE_BINARY_DIR}/counter.png" 1280 720
            --differs-from "${CMAKE_BINARY_DIR}/counter_zero.png")
    set_tests_properties(examples/counter_capture_shows_the_count PROPERTIES
        FIXTURES_REQUIRED arisa_counter_png)
endif()

add_test(NAME banned_symbols
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/banned_symbols.py"
        "${CMAKE_CURRENT_SOURCE_DIR}")
