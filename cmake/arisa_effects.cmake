add_custom_target(arisa_effect_registry)
set_property(TARGET arisa_effect_registry PROPERTY ARISA_EFFECTS "")
set_property(TARGET arisa_effect_registry PROPERTY ARISA_EFFECT_FILES "")
set_property(TARGET arisa_effect_registry PROPERTY ARISA_EFFECT_COUNT 0)
set_property(TARGET arisa_effect_registry PROPERTY ARISA_EFFECT_ENTRIES "")
set(ARISA_COMPOSE_EFFECTS_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/scripts/compose_effects.cmake")
set(arisa_effects_generated "${CMAKE_BINARY_DIR}/generated/effects")
set(ARISA_EFFECTS_LIST "${arisa_effects_generated}/effects.list")
set(ARISA_SHAPE_FRAGMENT "${arisa_effects_generated}/shape.frag")
set(ARISA_SHADER_REGISTRY "${arisa_effects_generated}/shader_registry.cpp")
file(GENERATE OUTPUT "${ARISA_SHADER_REGISTRY}"
    INPUT "${CMAKE_CURRENT_LIST_DIR}/shader_registry.cpp.in")
file(GENERATE OUTPUT "${ARISA_EFFECTS_LIST}"
    CONTENT "$<JOIN:$<TARGET_PROPERTY:arisa_effect_registry,ARISA_EFFECTS>,\n>\n")
add_custom_command(
    OUTPUT "${ARISA_SHAPE_FRAGMENT}"
    COMMAND "${CMAKE_COMMAND}"
        "-DBASE=${CMAKE_CURRENT_SOURCE_DIR}/shaders/shape.frag"
        "-DLIST=${ARISA_EFFECTS_LIST}"
        "-DOUTPUT=${ARISA_SHAPE_FRAGMENT}"
        -P "${ARISA_COMPOSE_EFFECTS_SCRIPT}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shape.frag"
        "${ARISA_EFFECTS_LIST}"
        "${ARISA_COMPOSE_EFFECTS_SCRIPT}"
        "$<TARGET_PROPERTY:arisa_effect_registry,ARISA_EFFECT_FILES>"
    VERBATIM)
