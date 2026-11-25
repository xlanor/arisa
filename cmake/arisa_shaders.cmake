set(ARISA_EMBED_SHADERS_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/scripts/embed_shaders.cmake")

function(arisa_add_shaders target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "HEADER;NAMESPACE;CLASS;INCLUDE" "SHADERS;EFFECTS")
    foreach(required HEADER NAMESPACE CLASS INCLUDE)
        if(NOT arg_${required})
            message(FATAL_ERROR "arisa_add_shaders(${target}) needs ${required}")
        endif()
    endforeach()
    if(NOT arg_SHADERS AND NOT arg_EFFECTS)
        message(FATAL_ERROR "arisa_add_shaders(${target}) needs SHADERS or EFFECTS")
    endif()
    if(ARISA_HORIZON_DEKO3D)
        if(NOT ARISA_DKSH_COMPILER)
            message(FATAL_ERROR "arisa_add_shaders(${target}) needs uam (uam2 for drako3d)")
        endif()
        set(extension dksh)
    else()
        find_program(ARISA_GLSLANG NAMES glslangValidator glslang REQUIRED)
        set(extension spv)
    endif()
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated/${target}")
    set(binaries)
    set(entries)
    foreach(shader IN LISTS arg_SHADERS)
        string(FIND "${shader}" "=" separator)
        if(separator EQUAL -1)
            message(FATAL_ERROR "arisa_add_shaders(${target}) expects name=path, got ${shader}")
        endif()
        string(SUBSTRING "${shader}" 0 ${separator} name)
        math(EXPR start "${separator} + 1")
        string(SUBSTRING "${shader}" ${start} -1 source)
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        cmake_path(GET source EXTENSION LAST_ONLY stage)
        string(SUBSTRING "${stage}" 1 -1 stage)
        cmake_path(GET source STEM file)
        set(binary "${generated}/${file}_${stage}.${extension}")
        if(extension STREQUAL "dksh")
            set(compile "${ARISA_DKSH_COMPILER}" -o "${binary}" -s ${stage} "${source}")
        else()
            set(compile "${ARISA_GLSLANG}" -V --target-env vulkan1.0 -S ${stage}
                -o "${binary}" "${source}")
        endif()
        add_custom_command(
            OUTPUT "${binary}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${generated}"
            COMMAND ${compile}
            DEPENDS "${source}"
            VERBATIM)
        list(APPEND binaries "${binary}")
        list(APPEND entries "${name}=${binary}")
    endforeach()
    set(library_sources)
    if(entries)
        list(JOIN entries "|" joined)
        set(embedded "${generated}/${target}.cpp")
        add_custom_command(
            OUTPUT "${embedded}"
            COMMAND "${CMAKE_COMMAND}"
                "-DHEADER=${arg_HEADER}"
                "-DNAMESPACE=${arg_NAMESPACE}"
                "-DCLASS=${arg_CLASS}"
                "-DSHADERS=${joined}"
                "-DOUTPUT=${embedded}"
                -P "${ARISA_EMBED_SHADERS_SCRIPT}"
            DEPENDS ${binaries} "${ARISA_EMBED_SHADERS_SCRIPT}"
            VERBATIM)
        list(APPEND library_sources "${embedded}")
    endif()
    if(arg_EFFECTS)
        if(NOT TARGET arisa_effect_registry)
            message(FATAL_ERROR "arisa_add_shaders(${target}) EFFECTS compile into arisa's shape shader, so arisa must be built in the same project")
        endif()
        set(functions "")
        foreach(effect IN LISTS arg_EFFECTS)
            string(REGEX MATCH "^([A-Za-z_][A-Za-z0-9_]*)=(colour|distortion|layer):(.+)$" matched "${effect}")
            if(NOT matched)
                message(FATAL_ERROR "arisa_add_shaders(${target}) expects EFFECTS name=colour|distortion|layer:path, got ${effect}")
            endif()
            set(name "${CMAKE_MATCH_1}")
            set(kind "${CMAKE_MATCH_2}")
            set(source "${CMAKE_MATCH_3}")
            cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
            get_property(index TARGET arisa_effect_registry PROPERTY ARISA_EFFECT_COUNT)
            math(EXPR next "${index} + 1")
            set_property(TARGET arisa_effect_registry PROPERTY ARISA_EFFECT_COUNT ${next})
            set_property(TARGET arisa_effect_registry APPEND PROPERTY ARISA_EFFECTS "${index}|${kind}|${name}|${source}")
            set_property(TARGET arisa_effect_registry APPEND PROPERTY ARISA_EFFECT_FILES "${source}")
            set_property(TARGET arisa_effect_registry APPEND PROPERTY ARISA_EFFECT_ENTRIES
                "      ShaderFunction{${index}, ShaderFunction::Kind::${kind}, \"${target}\", \"${name}\"},")
            string(APPEND functions "
auto ${arg_CLASS}::${name}() noexcept -> graphics::ShaderFunction {
  return graphics::ShaderFunction{${index}, graphics::ShaderFunction::Kind::${kind}, \"${target}\", \"${name}\"};
}
")
        endforeach()
        set(effects_source "${generated}/${target}_effects.cpp")
        file(MAKE_DIRECTORY "${generated}")
        file(CONFIGURE OUTPUT "${effects_source}" CONTENT "#include <${arg_HEADER}>

namespace ${arg_NAMESPACE} {
${functions}
}
" @ONLY)
        list(APPEND library_sources "${effects_source}")
    endif()
    add_library(${target} STATIC ${library_sources})
    if(arg_EFFECTS)
        target_link_libraries(${target} PUBLIC arisa_graphics)
    endif()
    cmake_path(ABSOLUTE_PATH arg_INCLUDE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(${target} PUBLIC "${arg_INCLUDE}")
    arisa_disable_exceptions_and_rtti(${target})
    set_target_properties(${target} PROPERTIES
        ARISA_SHADER_FORMAT "${extension}"
        ARISA_SHADER_BINARIES "${binaries}")
endfunction()
