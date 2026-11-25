option(ARISA_BUNDLE
    "Merge arisa and its vendored dependencies into one libarisa.a and install it as a CMake package"
    OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(arisa_generated_include "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
target_include_directories(arisa_support INTERFACE
    "$<BUILD_INTERFACE:${arisa_generated_include}>")

file(GLOB_RECURSE arisa_public_headers CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/arisa/*.hpp")
list(SORT arisa_public_headers)
set(arisa_excluded_headers "arisa/provider/diagnostics/allocation_meter.hpp")
set(arisa_core_headers ${arisa_public_headers})
list(FILTER arisa_core_headers EXCLUDE REGEX "^arisa/provider/(sdl3|horizon|vulkan)/")
list(FILTER arisa_core_headers EXCLUDE REGEX "^arisa/inspector/")
list(REMOVE_ITEM arisa_core_headers ${arisa_excluded_headers})

set(arisa_platform_headers)
set(arisa_platform_providers)
if(ARISA_PROVIDERS_SDL3)
    list(APPEND arisa_platform_providers sdl3)
endif()
if(ARISA_PROVIDERS_VULKAN)
    list(APPEND arisa_platform_providers vulkan)
endif()
if(ARISA_PROVIDERS_HORIZON)
    list(APPEND arisa_platform_providers horizon)
endif()
foreach(platform IN LISTS arisa_platform_providers)
    set(headers ${arisa_public_headers})
    list(FILTER headers INCLUDE REGEX "^arisa/provider/${platform}/")
    list(APPEND arisa_platform_headers ${headers})
endforeach()
if(ARISA_PROVIDERS_HORIZON AND ARISA_HORIZON_GPU STREQUAL "both")
    list(FILTER arisa_platform_headers EXCLUDE REGEX "^arisa/provider/horizon/nx/")
elseif(ARISA_PROVIDERS_HORIZON AND ARISA_HORIZON_GPU STREQUAL "drako3d")
    list(FILTER arisa_platform_headers EXCLUDE REGEX "^arisa/provider/horizon/(nx/|ounce/ounce_console\\.hpp)")
elseif(ARISA_PROVIDERS_HORIZON AND ARISA_HORIZON_CONSOLE STREQUAL "ounce")
    list(FILTER arisa_platform_headers EXCLUDE REGEX "^arisa/provider/horizon/(deko3d_[a-z_]+\\.hpp|nx/|ounce/drako_console\\.hpp)")
elseif(ARISA_PROVIDERS_HORIZON)
    list(FILTER arisa_platform_headers EXCLUDE REGEX "^arisa/provider/horizon/ounce/")
endif()

set(arisa_umbrella_headers ${arisa_core_headers})
list(FILTER arisa_umbrella_headers EXCLUDE REGEX
    "^arisa/(provider/(standard|headless)/|provider/default_factory\\.hpp$|conformance/)")
set(arisa_module_extra_headers ${arisa_core_headers})
list(REMOVE_ITEM arisa_module_extra_headers ${arisa_umbrella_headers})

set(arisa_umbrella "#pragma once\n\n")
foreach(header IN LISTS arisa_umbrella_headers)
    string(APPEND arisa_umbrella "#include <${header}>\n")
endforeach()
file(CONFIGURE OUTPUT "${arisa_generated_include}/arisa/arisa.hpp"
    CONTENT "${arisa_umbrella}" @ONLY)

set(arisa_module_externals)
foreach(header IN LISTS arisa_core_headers arisa_platform_headers)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/${header}" lines
        REGEX "^#include <[^>]+>")
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "^#include <([^>]+)>.*$" "\\1" included "${line}")
        if(NOT included MATCHES "^arisa/")
            list(APPEND arisa_module_externals "${included}")
        endif()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES arisa_module_externals)
list(SORT arisa_module_externals)

set(arisa_module_interface "module;\n\n")
foreach(included IN LISTS arisa_module_externals)
    string(APPEND arisa_module_interface "#include <${included}>\n")
endforeach()
string(APPEND arisa_module_interface "\nexport module arisa;\n\nexport extern \"C++\" {\n")
string(APPEND arisa_module_interface "#include <arisa/arisa.hpp>\n")
foreach(header IN LISTS arisa_module_extra_headers arisa_platform_headers)
    string(APPEND arisa_module_interface "#include <${header}>\n")
endforeach()
string(APPEND arisa_module_interface "}\n")
set(ARISA_MODULE_INTERFACE "${arisa_generated_include}/arisa/arisa.cppm")
file(CONFIGURE OUTPUT "${ARISA_MODULE_INTERFACE}"
    CONTENT "${arisa_module_interface}" @ONLY)

set(arisa_bundled_targets
    arisa_async
    arisa_async_threads
    arisa_provider
    arisa_provider_standard
    arisa_provider_headless
    arisa_provider_sdl3
    arisa_provider_vulkan
    arisa_provider_horizon
    arisa_conformance
    arisa_theme
    arisa_motion
    arisa_composition
    arisa_appearance
    arisa_text_input
    arisa_graphics
    arisa_text
    arisa_components
    arisa_interaction
    arisa_presentation
    arisa_controls
    arisa_structure
    arisa_layout
    arisa_rendering
    arisa_runtime
    arisa_stb_image
    arisa_freetype_vendor
    arisa_harfbuzz_vendor
    arisa_sheenbidi_vendor
    arisa_unibreak_vendor
    arisa_toml_vendor
    arisa_yoga_vendor
    arisa_nanovg_vendor
    arisa_libtess2_vendor)
set(arisa_vulkan_bundle_libraries)
if(ARISA_PROVIDERS_VULKAN AND NINTENDO_SWITCH2)
    set(arisa_vulkan_bundle_libraries vulkan)
elseif(ARISA_PROVIDERS_VULKAN)
    set(arisa_vulkan_bundle_libraries "$<BUILD_INTERFACE:${ARISA_VULKAN_LIBRARY}>")
endif()
set(arisa_horizon_bundle_libraries ${arisa_horizon_console_libraries})
if(ARISA_BUNDLE)
    list(TRANSFORM arisa_horizon_bundle_libraries REPLACE "^arisa_provider_vulkan$" "${arisa_vulkan_bundle_libraries}")
endif()

set(arisa_present_targets)
foreach(target IN LISTS arisa_bundled_targets)
    if(TARGET ${target})
        list(APPEND arisa_present_targets ${target})
    endif()
endforeach()

if(ARISA_BUNDLE)
    set(arisa_bundle_objects)
    foreach(target IN LISTS arisa_present_targets)
        list(APPEND arisa_bundle_objects "$<TARGET_OBJECTS:${target}>")
    endforeach()
    add_library(arisa STATIC ${arisa_bundle_objects})
    set_target_properties(arisa PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(arisa INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${arisa_generated_include}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
    if(ARISA_PROVIDERS_STANDARD)
        target_link_libraries(arisa INTERFACE Threads::Threads)
    endif()
    if(ARISA_PROVIDERS_HORIZON AND arisa_horizon_console_libraries)
        target_link_libraries(arisa INTERFACE ${arisa_horizon_bundle_libraries})
    endif()
    if(ARISA_PROVIDERS_SDL3)
        target_link_libraries(arisa INTERFACE "$<BUILD_INTERFACE:SDL3::SDL3>")
    endif()
else()
    add_library(arisa INTERFACE)
    set(arisa_source_targets ${arisa_present_targets})
    list(FILTER arisa_source_targets EXCLUDE REGEX "_vendor$|^arisa_stb_image$|^arisa_provider_(sdl3|horizon|vulkan)$")
    target_link_libraries(arisa INTERFACE ${arisa_source_targets})
endif()
target_compile_features(arisa INTERFACE cxx_std_23)
target_compile_options(arisa INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")
if(NINTENDO_SWITCH)
    target_compile_options(arisa INTERFACE ${ARISA_FRAME_POINTER_OPTIONS})
    if(ARISA_BUNDLE)
        target_link_options(arisa INTERFACE ${ARISA_CRASH_LINK_OPTIONS})
        target_compile_options(arisa INTERFACE
            "$<BUILD_INTERFACE:${ARISA_TRAP_OPTION}>"
            "$<INSTALL_INTERFACE:$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<BOOL:$<TARGET_PROPERTY:PRECOMPILE_HEADERS>>>>:SHELL:-include $<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}/arisa/provider/diagnostics/trap.h>>")
    endif()
endif()
add_library(arisa::arisa ALIAS arisa)

set(arisa_provider_targets standard)
if(ARISA_PROVIDERS_HEADLESS)
    list(APPEND arisa_provider_targets headless)
endif()
list(APPEND arisa_provider_targets ${arisa_platform_providers})
set(arisa_provider_dependencies_sdl3 "$<BUILD_INTERFACE:SDL3::SDL3>")
set(arisa_provider_dependencies_vulkan ${arisa_vulkan_bundle_libraries})
set(arisa_provider_dependencies_horizon ${arisa_horizon_bundle_libraries})
foreach(provider IN LISTS arisa_provider_targets)
    add_library(arisa_${provider} INTERFACE)
    set_target_properties(arisa_${provider} PROPERTIES EXPORT_NAME ${provider})
    target_link_libraries(arisa_${provider} INTERFACE arisa ${arisa_provider_dependencies_${provider}})
    if(NOT ARISA_BUNDLE)
        target_link_libraries(arisa_${provider} INTERFACE arisa_provider_${provider})
    endif()
    add_library(arisa::${provider} ALIAS arisa_${provider})
endforeach()
set(arisa_shim_install_dir "${CMAKE_INSTALL_INCLUDEDIR}/arisa/drako3d_shim")
if(ARISA_BUNDLE AND TARGET arisa_horizon)
    if(ARISA_HORIZON_DEKO3D)
        target_compile_definitions(arisa_horizon INTERFACE ARISA_HORIZON_DEKO3D=1)
    endif()
    if(ARISA_HORIZON_VULKAN)
        target_compile_definitions(arisa_horizon INTERFACE ARISA_HORIZON_VULKAN=1)
    endif()
    if(ARISA_DEKO3D_SHIM)
        target_include_directories(arisa_horizon INTERFACE
            "$<BUILD_INTERFACE:${ARISA_DEKO3D_SHIM}>"
            "$<INSTALL_INTERFACE:${arisa_shim_install_dir}>")
    endif()
endif()

if(ARISA_BUNDLE)
    if(NINTENDO_SWITCH2)
        set(ARISA_PACKAGE_PLATFORM switch2)
    elseif(NINTENDO_SWITCH)
        set(ARISA_PACKAGE_PLATFORM switch)
    elseif(APPLE)
        set(ARISA_PACKAGE_PLATFORM macos)
    elseif(WIN32)
        set(ARISA_PACKAGE_PLATFORM windows)
    else()
        set(ARISA_PACKAGE_PLATFORM linux)
    endif()
    set(ARISA_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/arisa")
    set(ARISA_INSTALL_ASSETSDIR "${CMAKE_INSTALL_DATADIR}/arisa/assets")
    set(ARISA_INSTALL_DOCDIR "${CMAKE_INSTALL_DATADIR}/doc/arisa")
    set(ARISA_PACKAGE_PROVIDERS ${arisa_provider_targets})
    set(ARISA_PACKAGE_PLATFORM_PROVIDERS ${arisa_platform_providers})

    set(arisa_provider_export_targets)
    foreach(provider IN LISTS arisa_provider_targets)
        list(APPEND arisa_provider_export_targets arisa_${provider})
    endforeach()
    install(TARGETS arisa ${arisa_provider_export_targets}
        EXPORT arisaTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(EXPORT arisaTargets
        NAMESPACE arisa::
        DESTINATION "${ARISA_INSTALL_CMAKEDIR}")

    set(arisa_installed_headers ${arisa_core_headers} ${arisa_platform_headers})
    foreach(header IN LISTS arisa_installed_headers)
        get_filename_component(directory "${header}" DIRECTORY)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/${header}"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${directory}")
    endforeach()
    install(FILES
        "${arisa_generated_include}/arisa/arisa.hpp"
        "${ARISA_MODULE_INTERFACE}"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/arisa")
    if(ARISA_PROVIDERS_HORIZON AND ARISA_DEKO3D_SHIM)
        install(FILES
            "${ARISA_DEKO3D_SHIM}/deko3d.h"
            "${ARISA_DEKO3D_SHIM}/deko3d.hpp"
            DESTINATION "${arisa_shim_install_dir}")
    endif()

    if(NINTENDO_SWITCH)
        install(FILES "${ARISA_TRAP_HEADER}"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/arisa/provider/diagnostics")
    endif()

    install(DIRECTORY
        "${ARISA_ASSETS_DIR}/arisa/fonts"
        "${ARISA_ASSETS_DIR}/arisa/themes"
        DESTINATION "${ARISA_INSTALL_ASSETSDIR}/arisa")

    set(arisa_third_party "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
    set(arisa_notices_sources
        "Yoga|${arisa_third_party}/yoga/LICENSE"
        "HarfBuzz|${arisa_third_party}/harfbuzz/COPYING"
        "FreeType (used under the FreeType License)|${arisa_third_party}/freetype/docs/FTL.TXT"
        "SheenBidi|${arisa_third_party}/sheenbidi/LICENSE"
        "libunibreak|${arisa_third_party}/libunibreak/LICENCE"
        "Boost.SML|${arisa_third_party}/sml/LICENSE.md"
        "toml++|${arisa_third_party}/tomlplusplus/LICENSE"
        "NanoVG (path stroker)|${arisa_third_party}/nanovg/LICENSE.txt"
        "libtess2|${arisa_third_party}/libtess2/LICENSE.txt"
        "Inter (share/arisa/assets/arisa/fonts/InterVariable.ttf)|${ARISA_ASSETS_DIR}/arisa/fonts/InterVariable-OFL.txt"
        "Inter Italic (share/arisa/assets/arisa/fonts/InterItalic-*.ttf)|${ARISA_ASSETS_DIR}/arisa/fonts/InterItalic-OFL.txt"
        "JetBrains Mono (share/arisa/assets/arisa/fonts/JetBrainsMono-*.ttf)|${ARISA_ASSETS_DIR}/arisa/fonts/JetBrainsMono-OFL.txt"
        "Noto Serif (share/arisa/assets/arisa/fonts/NotoSerif-*.ttf)|${ARISA_ASSETS_DIR}/arisa/fonts/NotoSerif-OFL.txt"
        "Nunito (share/arisa/assets/arisa/fonts/Nunito-*.ttf)|${ARISA_ASSETS_DIR}/arisa/fonts/Nunito-OFL.txt"
        "Material Symbols (share/arisa/assets/arisa/fonts/MaterialSymbolsRounded.ttf, MaterialSymbolsRoundedFilled.ttf)|${ARISA_ASSETS_DIR}/arisa/fonts/MaterialSymbolsRounded-LICENSE.txt")
    set(arisa_notices "arisa ${PROJECT_VERSION} for ${ARISA_PACKAGE_PLATFORM} bundles the following third-party software in lib/libarisa.a and share/arisa/assets.\n")
    foreach(entry IN LISTS arisa_notices_sources)
        string(FIND "${entry}" "|" separator)
        string(SUBSTRING "${entry}" 0 ${separator} name)
        math(EXPR start "${separator} + 1")
        string(SUBSTRING "${entry}" ${start} -1 path)
        file(READ "${path}" licence)
        string(APPEND arisa_notices "\n================================================================================\n${name}\n================================================================================\n\n${licence}\n")
    endforeach()
    file(READ "${arisa_third_party}/stb/stb_image.h" stb_source)
    string(FIND "${stb_source}" "This software is available under 2 licenses" stb_start)
    string(SUBSTRING "${stb_source}" ${stb_start} -1 stb_licence)
    string(FIND "${stb_licence}" "*/" stb_end)
    string(SUBSTRING "${stb_licence}" 0 ${stb_end} stb_licence)
    string(APPEND arisa_notices "\n================================================================================\nstb_image\n================================================================================\n\n${stb_licence}\n")
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/THIRD_PARTY_NOTICES.txt"
        CONTENT "${arisa_notices}" @ONLY)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/THIRD_PARTY_NOTICES.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${ARISA_INSTALL_DOCDIR}")

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arisaConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/arisaConfig.cmake"
        INSTALL_DESTINATION "${ARISA_INSTALL_CMAKEDIR}"
        PATH_VARS CMAKE_INSTALL_INCLUDEDIR ARISA_INSTALL_ASSETSDIR ARISA_INSTALL_DOCDIR)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/arisaConfigVersion.cmake"
        COMPATIBILITY SameMinorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/arisaConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/arisaConfigVersion.cmake"
        DESTINATION "${ARISA_INSTALL_CMAKEDIR}")
endif()
