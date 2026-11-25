option(ARISA_PROVIDERS_STANDARD
    "Build the standard library providers and DefaultProviderFactory" ON)

if(ARISA_PROVIDERS_STANDARD)
    find_package(Threads REQUIRED)
    add_library(arisa_provider_standard STATIC
        src/provider/standard/default_factory.cpp
        src/provider/standard/directory_assets.cpp
        src/provider/standard/directory_storage.cpp
        src/provider/standard/file_stat.cpp
        src/provider/standard/standard_clock.cpp
        src/provider/standard/std_threading.cpp
        src/provider/standard/stderr_diagnostics.cpp)
    target_link_libraries(arisa_provider_standard
        PUBLIC arisa_provider
        PRIVATE Threads::Threads)
    arisa_enable_strict_warnings(arisa_provider_standard)
endif()
