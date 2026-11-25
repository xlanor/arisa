option(ARISA_BUILD_EXAMPLES "Build the examples" ON)

if(ARISA_BUILD_EXAMPLES)
    add_library(arisa_counter_screen STATIC
        examples/counter/counter_screen.cpp)
    target_include_directories(arisa_counter_screen PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/counter")
    target_link_libraries(arisa_counter_screen PUBLIC arisa_components arisa_runtime arisa_text)
    arisa_enable_strict_warnings(arisa_counter_screen)

    add_library(arisa_gallery_screen STATIC
        examples/gallery/launch_screen.cpp
        examples/gallery/colours_page.cpp
        examples/gallery/controls_page.cpp
        examples/gallery/covers_page.cpp
        examples/gallery/crashes_page.cpp
        examples/gallery/effects_page.cpp
        examples/gallery/feed_page.cpp
        examples/gallery/library_page.cpp
        examples/gallery/row_actions_page.cpp
        examples/gallery/feedback_page.cpp
        examples/gallery/fidelity_page.cpp
        examples/gallery/focus_page.cpp
        examples/gallery/frame_meter.cpp
        examples/gallery/gallery_fonts.cpp
        examples/gallery/gallery_page.cpp
        examples/gallery/gallery_photo.cpp
        examples/gallery/gallery_screen.cpp
        examples/gallery/gallery_wallpaper.cpp
        examples/gallery/glass_page.cpp
        examples/gallery/glass_preview.cpp
        examples/gallery/glass_scene.cpp
        examples/gallery/materials_page.cpp
        examples/gallery/icons_page.cpp
        examples/gallery/input_block_demo.cpp
        examples/gallery/input_blocker.cpp
        examples/gallery/graphics_page.cpp
        examples/gallery/shapes_page.cpp
        examples/gallery/canvas_page.cpp
        examples/gallery/springs_page.cpp
        examples/gallery/assistive_page.cpp
        examples/gallery/documents_page.cpp
        examples/gallery/animators_page.cpp
        examples/gallery/groupings_page.cpp
        examples/gallery/input_page.cpp
        examples/gallery/pointer_page.cpp
        examples/gallery/tables_page.cpp
        examples/gallery/symbols_page.cpp
        examples/gallery/text_formats_page.cpp
        examples/gallery/layout_scroll_page.cpp
        examples/gallery/interaction_page.cpp
        examples/gallery/text_editing_page.cpp
        examples/gallery/launch_page.cpp
        examples/gallery/lists_page.cpp
        examples/gallery/motion_page.cpp
        examples/gallery/chrome_page.cpp
        examples/gallery/navigation_page.cpp
        examples/gallery/panels_page.cpp
        examples/gallery/paths_page.cpp
        examples/gallery/presentations_page.cpp
        examples/gallery/profile_form.cpp
        examples/gallery/profile_summary.cpp
        examples/gallery/scroll_page.cpp
        examples/gallery/scroll_views_page.cpp
        examples/gallery/custom_layout_page.cpp
        examples/gallery/world_page.cpp
        examples/gallery/rich_text_page.cpp
        examples/gallery/data_flow_page.cpp
        examples/gallery/geometry_page.cpp
        examples/gallery/search_page.cpp
        examples/gallery/settings_page.cpp
        examples/gallery/text_input_demo.cpp
        examples/gallery/theming_page.cpp
        examples/gallery/typography_page.cpp)
    target_include_directories(arisa_gallery_screen PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/gallery")
    arisa_add_shaders(arisa_gallery_shaders
        HEADER gallery_shaders.hpp
        NAMESPACE arisa::examples
        CLASS GalleryShaders
        INCLUDE examples/gallery
        EFFECTS
            ripple=distortion:examples/gallery/ripple.glsl
            stripes=colour:examples/gallery/stripes.glsl
            bands=colour:examples/gallery/bands.glsl)
    target_link_libraries(arisa_gallery_screen PUBLIC arisa_components arisa_controls arisa_markup arisa_text arisa_theme arisa_appearance
            arisa_runtime arisa_gallery_shaders
        PRIVATE arisa_stb_image_write)
    arisa_enable_strict_warnings(arisa_gallery_screen)

    add_library(arisa_gpu_stage_screen STATIC
        examples/gpu_stage/gpu_stage_screen.cpp
        examples/gpu_stage/stage_control.cpp)
    target_include_directories(arisa_gpu_stage_screen PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/gpu_stage")
    target_link_libraries(arisa_gpu_stage_screen PUBLIC arisa_components arisa_controls arisa_theme arisa_provider)
    arisa_enable_strict_warnings(arisa_gpu_stage_screen)

    add_library(arisa_sample_app STATIC
        examples/sample/animation.cpp
        examples/sample/sample_model.cpp
        examples/sample/sample_screen.cpp)
    target_include_directories(arisa_sample_app PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/sample")
    target_link_libraries(arisa_sample_app PUBLIC arisa_components arisa_theme arisa_provider)
    arisa_enable_strict_warnings(arisa_sample_app)
endif()

if(ARISA_BUILD_EXAMPLES AND (ARISA_PROVIDERS_VULKAN OR
        (ARISA_PROVIDERS_SDL3 AND ARISA_PROVIDERS_HEADLESS) OR
        (ARISA_PROVIDERS_HORIZON AND ARISA_HORIZON_DEKO3D)))
    arisa_add_shaders(arisa_plasma_shaders
        HEADER plasma_shaders.hpp
        NAMESPACE arisa::examples
        CLASS PlasmaShaders
        INCLUDE examples/gpu_stage
        SHADERS
            vertex=examples/gpu_stage/plasma.vert
            fragment=examples/gpu_stage/plasma.frag)
endif()

if(ARISA_BUILD_EXAMPLES AND ARISA_PROVIDERS_VULKAN)
    add_library(arisa_gpu_stage_vulkan STATIC
        examples/gpu_stage/decoded_frame.cpp
        examples/gpu_stage/h264_annex_b.cpp
        examples/gpu_stage/vulkan_decoded_video.cpp
        examples/gpu_stage/vulkan_nv12_video.cpp
        examples/gpu_stage/vulkan_plasma.cpp
        examples/gpu_stage/vulkan_plasma_program.cpp
        examples/gpu_stage/vulkan_recorder.cpp
        examples/gpu_stage/vulkan_timeline.cpp
        examples/gpu_stage/vulkan_video.cpp)
    target_link_libraries(arisa_gpu_stage_vulkan PUBLIC
        arisa_gpu_stage_screen arisa_plasma_shaders arisa_provider_vulkan)
    arisa_enable_strict_warnings(arisa_gpu_stage_vulkan)

    add_library(arisa_sample_vulkan STATIC examples/sample/vulkan_sample_renderer.cpp)
    target_link_libraries(arisa_sample_vulkan PUBLIC arisa_sample_app arisa_gpu_stage_vulkan)
    arisa_enable_strict_warnings(arisa_sample_vulkan)
endif()

if(ARISA_BUILD_EXAMPLES AND ARISA_PROVIDERS_HEADLESS)
    add_library(arisa_bench_scenarios STATIC
        examples/bench/bench_screen.cpp
        examples/bench/benchmark.cpp
        examples/bench/interaction_screen.cpp
        examples/bench/soak.cpp
        examples/bench/soak_lifecycle.cpp
        examples/bench/soak_screen.cpp
        examples/bench/split_screen.cpp)
    target_include_directories(arisa_bench_scenarios PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/bench")
    target_link_libraries(arisa_bench_scenarios PUBLIC
        arisa_gallery_screen arisa_runtime arisa_provider_headless
        arisa_allocation_meter)
    if(TARGET arisa_inspector)
        target_link_libraries(arisa_bench_scenarios PUBLIC arisa_inspector)
    endif()
    arisa_enable_strict_warnings(arisa_bench_scenarios)

    if(NOT NINTENDO_SWITCH)
        add_library(arisa_bench_host STATIC examples/bench/host_assets.cpp
            examples/bench/real_time_providers.cpp)
        target_include_directories(arisa_bench_host PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/bench")
        target_compile_definitions(arisa_bench_host PRIVATE
            ARISA_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets"
            ARISA_LETTERING_FONT="${ARISA_LETTERING_FONT}")
        target_link_libraries(arisa_bench_host PUBLIC arisa_provider_headless
            arisa_provider_standard PRIVATE arisa_gallery_screen)
        arisa_enable_strict_warnings(arisa_bench_host)

        add_executable(arisa_bench examples/bench/main.cpp)
        target_link_libraries(arisa_bench PRIVATE
            arisa_bench_scenarios arisa_bench_host arisa_provider_standard)
        arisa_enable_strict_warnings(arisa_bench)

        add_executable(arisa_soak examples/bench/soak_main.cpp)
        target_link_libraries(arisa_soak PRIVATE
            arisa_bench_scenarios arisa_bench_host arisa_provider_standard)
        arisa_enable_strict_warnings(arisa_soak)
    endif()
endif()

if(ARISA_BUILD_EXAMPLES)
    add_library(arisa_stb_image_write STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/stb_image_write.cpp")
    target_include_directories(arisa_stb_image_write SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb")
    arisa_disable_exceptions_and_rtti(arisa_stb_image_write)
endif()

if(ARISA_BUILD_EXAMPLES AND ARISA_PROVIDERS_SDL3 AND ARISA_PROVIDERS_HEADLESS)

    add_library(arisa_desktop_example STATIC
        examples/common/desktop_example.cpp
        examples/common/example_options.cpp)
    target_include_directories(arisa_desktop_example PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/common")
    target_link_libraries(arisa_desktop_example
        PUBLIC arisa_provider_sdl3
        PRIVATE arisa_stb_image_write)
    if(ARISA_VULKAN_LOADER)
        target_compile_definitions(arisa_desktop_example PRIVATE
            "ARISA_VULKAN_LOADER=\"${ARISA_VULKAN_LOADER}\"")
    endif()
    arisa_enable_strict_warnings(arisa_desktop_example)

    add_executable(arisa_glass_reference examples/glass_reference/main.cpp)
    set_target_properties(arisa_glass_reference PROPERTIES OUTPUT_NAME glass_reference)
    target_link_libraries(arisa_glass_reference PRIVATE
        arisa_gallery_screen arisa_runtime arisa_desktop_example)
    target_compile_definitions(arisa_glass_reference PRIVATE
        ARISA_GLASS_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/swiftui_reference/glass")
    arisa_enable_strict_warnings(arisa_glass_reference)

    add_executable(arisa_counter examples/counter/main.cpp)
    set_target_properties(arisa_counter PROPERTIES OUTPUT_NAME counter)
    target_link_libraries(arisa_counter PRIVATE
        arisa_counter_screen arisa_runtime arisa_desktop_example
        arisa_provider_headless)
    arisa_enable_strict_warnings(arisa_counter)

    add_executable(arisa_gpu_stage examples/gpu_stage/main.cpp
        examples/gpu_stage/sdl_plasma.cpp
        examples/gpu_stage/sdl_video.cpp
        examples/gpu_stage/sdl_nv12_video.cpp)
    set_target_properties(arisa_gpu_stage PROPERTIES OUTPUT_NAME gpu_stage)
    target_link_libraries(arisa_gpu_stage PRIVATE
        arisa_gpu_stage_screen arisa_plasma_shaders arisa_runtime
        arisa_desktop_example)
    arisa_enable_strict_warnings(arisa_gpu_stage)
    if(TARGET arisa_inspector)
        target_link_libraries(arisa_gpu_stage PRIVATE arisa_inspector)
    endif()

    add_library(arisa_sample_sdl STATIC examples/sample/sdl_sample_renderer.cpp)
    target_link_libraries(arisa_sample_sdl PUBLIC
        arisa_sample_app arisa_plasma_shaders arisa_provider_sdl3)
    arisa_enable_strict_warnings(arisa_sample_sdl)

    add_executable(arisa_sample examples/sample/main.cpp)
    set_target_properties(arisa_sample PROPERTIES OUTPUT_NAME sample)
    target_link_libraries(arisa_sample PRIVATE
        arisa_sample_sdl arisa_runtime arisa_desktop_example)
    arisa_enable_strict_warnings(arisa_sample)
    if(TARGET arisa_inspector)
        target_link_libraries(arisa_sample PRIVATE arisa_inspector)
    endif()

    add_executable(arisa_gallery examples/gallery/main.cpp
        examples/gallery/capture_clock.cpp)
    set_target_properties(arisa_gallery PROPERTIES OUTPUT_NAME gallery)
    target_link_libraries(arisa_gallery PRIVATE
        arisa_gallery_screen arisa_runtime arisa_desktop_example
        arisa_provider_headless arisa_provider_standard)
    arisa_enable_strict_warnings(arisa_gallery)
    if(TARGET arisa_inspector)
        target_link_libraries(arisa_gallery PRIVATE arisa_inspector)
    endif()
endif()

if(ARISA_BUILD_EXAMPLES AND ARISA_PROVIDERS_HORIZON)
    add_library(arisa_switch_console STATIC switch/switch_console.cpp)
    target_include_directories(arisa_switch_console PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/switch")
    target_link_libraries(arisa_switch_console PUBLIC arisa_provider_horizon)
    arisa_enable_strict_warnings(arisa_switch_console)

    add_executable(arisa_counter_switch switch/counter_main.cpp)
    target_link_libraries(arisa_counter_switch PRIVATE
        arisa_counter_screen arisa_runtime arisa_switch_console arisa_switch_log)
    arisa_enable_strict_warnings(arisa_counter_switch)
    nx_generate_nacp(arisa_counter.nacp
        NAME "arisa counter${ARISA_NACP_SUFFIX}"
        AUTHOR "xlanor"
        VERSION "${PROJECT_VERSION}")
    nx_create_nro(arisa_counter_switch
        NACP arisa_counter.nacp
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arisa_counter.nro"
        ROMFS "${ARISA_ROMFS_DIR}")

endif()

if(ARISA_BUILD_EXAMPLES AND ARISA_PROVIDERS_HORIZON AND ARISA_HORIZON_DEKO3D)
    set(arisa_gpu_stage_romfs "${ARISA_ROMFS_DIR}")
    set(arisa_gpu_stage_decoder)
    if(NINTENDO_SWITCH2)
        set(arisa_gpu_stage_romfs "${CMAKE_CURRENT_BINARY_DIR}/romfs_gpu_stage")
        file(COPY "${ARISA_ROMFS_DIR}/" DESTINATION "${arisa_gpu_stage_romfs}")
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/examples/gpu_stage/media/stage.h264"
            DESTINATION "${arisa_gpu_stage_romfs}/arisa/video")
        set(arisa_gpu_stage_decoder
            examples/gpu_stage/decoded_frame.cpp
            examples/gpu_stage/h264_annex_b.cpp
            examples/gpu_stage/ounce_frame_decoder.cpp
            examples/gpu_stage/deko3d_decoded_video.cpp)
    endif()
    add_executable(arisa_gpu_stage_switch switch/gpu_stage_main.cpp
        examples/gpu_stage/deko3d_plasma.cpp
        examples/gpu_stage/deko3d_video.cpp
        examples/gpu_stage/deko3d_nv12_video.cpp
        ${arisa_gpu_stage_decoder})
    target_link_libraries(arisa_gpu_stage_switch PRIVATE
        arisa_gpu_stage_screen arisa_plasma_shaders arisa_runtime arisa_switch_console arisa_switch_log)
    arisa_enable_strict_warnings(arisa_gpu_stage_switch)
    if(TARGET arisa_inspector)
        target_link_libraries(arisa_gpu_stage_switch PRIVATE arisa_inspector)
    endif()
    nx_generate_nacp(arisa_gpu_stage.nacp
        NAME "arisa gpu stage${ARISA_NACP_SUFFIX}"
        AUTHOR "xlanor"
        VERSION "${PROJECT_VERSION}")
    nx_create_nro(arisa_gpu_stage_switch
        NACP arisa_gpu_stage.nacp
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arisa_gpu_stage.nro"
        ROMFS "${arisa_gpu_stage_romfs}")

    add_executable(arisa_sample_switch examples/sample/switch_main.cpp
        examples/sample/deko3d_sample_renderer.cpp)
    target_link_libraries(arisa_sample_switch PRIVATE
        arisa_sample_app arisa_plasma_shaders arisa_runtime arisa_switch_console arisa_switch_log)
endif()

if(ARISA_BUILD_EXAMPLES AND ARISA_PROVIDERS_HORIZON AND NOT ARISA_HORIZON_DEKO3D)
    set(arisa_gpu_stage_romfs "${CMAKE_CURRENT_BINARY_DIR}/romfs_gpu_stage")
    file(COPY "${ARISA_ROMFS_DIR}/" DESTINATION "${arisa_gpu_stage_romfs}")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/examples/gpu_stage/media/stage.h264"
        DESTINATION "${arisa_gpu_stage_romfs}/arisa/video")
    add_executable(arisa_gpu_stage_switch switch/gpu_stage_main.cpp
        examples/gpu_stage/ounce_frame_decoder.cpp)
    target_link_libraries(arisa_gpu_stage_switch PRIVATE
        arisa_gpu_stage_vulkan arisa_runtime arisa_switch_console arisa_switch_log)
    arisa_enable_strict_warnings(arisa_gpu_stage_switch)
    if(TARGET arisa_inspector)
        target_link_libraries(arisa_gpu_stage_switch PRIVATE arisa_inspector)
    endif()
    nx_generate_nacp(arisa_gpu_stage.nacp
        NAME "arisa gpu stage${ARISA_NACP_SUFFIX}"
        AUTHOR "xlanor"
        VERSION "${PROJECT_VERSION}")
    nx_create_nro(arisa_gpu_stage_switch
        NACP arisa_gpu_stage.nacp
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arisa_gpu_stage.nro"
        ROMFS "${arisa_gpu_stage_romfs}")

    add_executable(arisa_sample_switch examples/sample/switch_main.cpp)
    target_link_libraries(arisa_sample_switch PRIVATE
        arisa_sample_vulkan arisa_runtime arisa_switch_console arisa_switch_log)
endif()

if(ARISA_BUILD_EXAMPLES AND ARISA_PROVIDERS_HORIZON)
    arisa_enable_strict_warnings(arisa_sample_switch)
    if(TARGET arisa_inspector)
        target_link_libraries(arisa_sample_switch PRIVATE arisa_inspector)
    endif()
    nx_generate_nacp(arisa_sample.nacp
        NAME "arisa sample${ARISA_NACP_SUFFIX}"
        AUTHOR "xlanor"
        VERSION "${PROJECT_VERSION}")
    nx_create_nro(arisa_sample_switch
        NACP arisa_sample.nacp
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arisa_sample.nro"
        ROMFS "${ARISA_ROMFS_DIR}")
endif()

if(ARISA_BUILD_EXAMPLES AND ARISA_PROVIDERS_HORIZON)
    add_executable(arisa_gallery_switch switch/gallery_main.cpp
        switch/display_report.cpp switch/frame_capture.cpp)
    target_include_directories(arisa_gallery_switch PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/switch")
    target_link_libraries(arisa_gallery_switch PRIVATE
        arisa_gallery_screen arisa_runtime arisa_switch_console arisa_switch_log
        arisa_stb_image_write arisa_provider_standard arisa_allocation_meter)
    arisa_enable_strict_warnings(arisa_gallery_switch)
    if(TARGET arisa_inspector)
        target_link_libraries(arisa_gallery_switch PRIVATE arisa_inspector)
    endif()
    nx_generate_nacp(arisa_gallery.nacp
        NAME "arisa gallery${ARISA_NACP_SUFFIX}"
        AUTHOR "xlanor"
        VERSION "${PROJECT_VERSION}")
    nx_create_nro(arisa_gallery_switch
        NACP arisa_gallery.nacp
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arisa_gallery.nro"
        ROMFS "${ARISA_ROMFS_DIR}")

    if(TARGET arisa_bench_scenarios)
        add_executable(arisa_bench_switch switch/bench_main.cpp)
        target_include_directories(arisa_bench_switch PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/switch")
        target_link_libraries(arisa_bench_switch PRIVATE
            arisa_bench_scenarios arisa_switch_console arisa_switch_log
            arisa_provider_standard)
        arisa_enable_strict_warnings(arisa_bench_switch)
        nx_generate_nacp(arisa_bench.nacp
            NAME "arisa bench${ARISA_NACP_SUFFIX}"
            AUTHOR "xlanor"
            VERSION "${PROJECT_VERSION}")
        nx_create_nro(arisa_bench_switch
            NACP arisa_bench.nacp
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arisa_bench.nro"
            ROMFS "${ARISA_ROMFS_DIR}")

        add_executable(arisa_soak_switch switch/soak_main.cpp)
        target_include_directories(arisa_soak_switch PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/switch")
        target_link_libraries(arisa_soak_switch PRIVATE
            arisa_bench_scenarios arisa_switch_console arisa_switch_log
            arisa_provider_standard)
        arisa_enable_strict_warnings(arisa_soak_switch)
        nx_generate_nacp(arisa_soak.nacp
            NAME "arisa soak${ARISA_NACP_SUFFIX}"
            AUTHOR "xlanor"
            VERSION "${PROJECT_VERSION}")
        nx_create_nro(arisa_soak_switch
            NACP arisa_soak.nacp
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/arisa_soak.nro"
            ROMFS "${ARISA_ROMFS_DIR}")
    endif()
endif()
