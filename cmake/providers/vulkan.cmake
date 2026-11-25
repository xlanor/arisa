set(arisa_providers_vulkan_default OFF)
if(NINTENDO_SWITCH2)
    set(arisa_providers_vulkan_default ON)
elseif(NOT NINTENDO_SWITCH)
    find_path(ARISA_VULKAN_INCLUDE_DIR vulkan/vulkan.h)
    find_library(ARISA_VULKAN_LIBRARY NAMES vulkan.1 vulkan)
    find_program(ARISA_GLSLANG NAMES glslangValidator glslang)
    if(ARISA_VULKAN_INCLUDE_DIR AND ARISA_VULKAN_LIBRARY AND ARISA_GLSLANG)
        set(arisa_providers_vulkan_default ON)
    endif()
endif()

option(ARISA_PROVIDERS_VULKAN
    "Build the Vulkan GPU provider (OunceVK on Switch 2, the system loader on desktop)"
    ${arisa_providers_vulkan_default})

if(ARISA_PROVIDERS_VULKAN)
    if(NINTENDO_SWITCH AND NOT NINTENDO_SWITCH2)
        message(FATAL_ERROR "ARISA_PROVIDERS_VULKAN needs libounce's Switch2.cmake on a console; Switch 1 uses deko3d")
    endif()
    find_program(ARISA_GLSLANG NAMES glslangValidator glslang REQUIRED)
    set(arisa_vulkan_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/vulkan")
    set(arisa_vulkan_sources
        "shape_vert=vert=${CMAKE_CURRENT_SOURCE_DIR}/shaders/shape.vert"
        "shape_frag=frag=${ARISA_SHAPE_FRAGMENT}"
        "textured_vert=vert=${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured.vert"
        "textured_frag=frag=${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured.frag")
    set(arisa_vulkan_binaries)
    foreach(entry IN LISTS arisa_vulkan_sources)
        string(REPLACE "=" ";" parts "${entry}")
        list(GET parts 0 name)
        list(GET parts 1 stage)
        list(GET parts 2 source)
        add_custom_command(
            OUTPUT "${arisa_vulkan_generated}/${name}.spv"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${arisa_vulkan_generated}"
            COMMAND "${ARISA_GLSLANG}" -V --target-env vulkan1.0 -DARISA_PUSH_CONSTANTS
                -S ${stage} -o "${arisa_vulkan_generated}/${name}.spv" "${source}"
            DEPENDS "${source}"
            VERBATIM)
        list(APPEND arisa_vulkan_binaries "${arisa_vulkan_generated}/${name}.spv")
    endforeach()
    add_custom_command(
        OUTPUT "${arisa_vulkan_generated}/vulkan_shaders.cpp"
        COMMAND "${CMAKE_COMMAND}"
            "-DHEADER=provider/vulkan/vulkan_shaders.hpp"
            "-DNAMESPACE=arisa::provider::vulkan"
            "-DCLASS=VulkanShaders"
            "-DSHADERS=shape_vertex=${arisa_vulkan_generated}/shape_vert.spv|shape_fragment=${arisa_vulkan_generated}/shape_frag.spv|textured_vertex=${arisa_vulkan_generated}/textured_vert.spv|textured_fragment=${arisa_vulkan_generated}/textured_frag.spv"
            "-DOUTPUT=${arisa_vulkan_generated}/vulkan_shaders.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts/embed_shaders.cmake"
        DEPENDS
            ${arisa_vulkan_binaries}
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts/embed_shaders.cmake"
        VERBATIM)

    add_library(arisa_provider_vulkan STATIC
        src/provider/vulkan/descriptor_pools.cpp
        src/provider/vulkan/frame_slot.cpp
        src/provider/vulkan/image_barriers.cpp
        src/provider/vulkan/memory_allocator.cpp
        src/provider/vulkan/offscreen_target.cpp
        src/provider/vulkan/present_mode.cpp
        src/provider/vulkan/resident_block.cpp
        src/provider/vulkan/shape_pipelines.cpp
        src/provider/vulkan/swapchain_target.cpp
        src/provider/vulkan/timestamp_queries.cpp
        src/provider/vulkan/vulkan_buffer.cpp
        src/provider/vulkan/vulkan_device.cpp
        src/provider/vulkan/vulkan_gpu.cpp
        src/provider/vulkan/vulkan_host_image.cpp
        src/provider/vulkan/vulkan_image.cpp
        src/provider/vulkan/vulkan_instance.cpp
        src/provider/vulkan/vulkan_surface.cpp
        "${arisa_vulkan_generated}/vulkan_shaders.cpp")
    target_include_directories(arisa_provider_vulkan PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(arisa_provider_vulkan PRIVATE VK_ENABLE_BETA_EXTENSIONS)
    target_link_libraries(arisa_provider_vulkan PUBLIC arisa_provider)
    if(NINTENDO_SWITCH2)
        target_link_libraries(arisa_provider_vulkan PUBLIC vulkan)
    else()
        target_include_directories(arisa_provider_vulkan SYSTEM PUBLIC
            "${ARISA_VULKAN_INCLUDE_DIR}")
        target_link_libraries(arisa_provider_vulkan PUBLIC "${ARISA_VULKAN_LIBRARY}")
    endif()
    arisa_enable_strict_warnings(arisa_provider_vulkan)
endif()
