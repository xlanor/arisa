option(ARISA_PROVIDERS_HEADLESS
    "Build the headless providers and HeadlessProviderFactory" ON)

if(ARISA_PROVIDERS_HEADLESS AND NOT ARISA_PROVIDERS_STANDARD)
    message(FATAL_ERROR
        "ARISA_PROVIDERS_HEADLESS runs on standard threads and needs ARISA_PROVIDERS_STANDARD")
endif()

if(ARISA_PROVIDERS_HEADLESS)
    add_library(arisa_provider_headless STATIC
        src/provider/headless/headless_factory.cpp
        src/provider/headless/manual_clock.cpp
        src/provider/headless/memory_assets.cpp
        src/provider/headless/memory_storage.cpp
        src/provider/headless/memory_clipboard.cpp
        src/provider/headless/memory_fonts.cpp
        src/provider/headless/null_gpu.cpp
        src/provider/headless/recording_audio.cpp
        src/provider/headless/recording_speech.cpp
        src/provider/headless/recording_browser.cpp
        src/provider/headless/scripted_applets.cpp
        src/provider/headless/recording_diagnostics.cpp
        src/provider/headless/recording_haptics.cpp
        src/provider/headless/scripted_input.cpp
        src/provider/headless/scripted_lifecycle.cpp
        src/provider/headless/scripted_surface.cpp
        src/provider/headless/scripted_system_info.cpp
        src/provider/headless/scripted_text_input.cpp
        src/provider/headless/software_raster.cpp)
    target_link_libraries(arisa_provider_headless
        PUBLIC arisa_provider arisa_provider_standard)
    arisa_enable_strict_warnings(arisa_provider_headless)
endif()
