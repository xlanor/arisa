cmake_minimum_required(VERSION 3.28)

project(arisa_consumer VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARISA_CONSUMER_MODULE "Build the consumer with import arisa; instead of the umbrella header" OFF)

set(consumer_components headless)
if(NINTENDO_SWITCH)
    list(APPEND consumer_components horizon)
endif()
if(ARISA_CONSUMER_MODULE)
    list(APPEND consumer_components module)
endif()

find_package(arisa 0.0 REQUIRED COMPONENTS ${consumer_components} OPTIONAL_COMPONENTS sdl3)

add_executable(arisa_consumer main.cpp)
target_link_libraries(arisa_consumer PRIVATE arisa::headless)
if(NINTENDO_SWITCH)
    target_link_libraries(arisa_consumer PRIVATE arisa::horizon)
elseif(arisa_sdl3_FOUND)
    target_link_libraries(arisa_consumer PRIVATE arisa::sdl3)
    target_compile_definitions(arisa_consumer PRIVATE ARISA_CONSUMER_WINDOW)
endif()
if(ARISA_CONSUMER_MODULE)
    target_link_libraries(arisa_consumer PRIVATE arisa::module)
    target_compile_definitions(arisa_consumer PRIVATE ARISA_CONSUMER_MODULE)
else()
    set_target_properties(arisa_consumer PROPERTIES CXX_SCAN_FOR_MODULES OFF)
endif()

if(NINTENDO_SWITCH)
    nx_generate_nacp(arisa_consumer.nacp
        NAME "arisa consumer"
        AUTHOR "arisa"
        VERSION "${PROJECT_VERSION}")
    nx_create_nro(arisa_consumer
        NACP arisa_consumer.nacp
        ROMFS "${arisa_ASSETS_DIR}")
else()
    add_custom_command(TARGET arisa_consumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${arisa_ASSETS_DIR}" "$<TARGET_FILE_DIR:arisa_consumer>/assets"
        VERBATIM)
endif()
