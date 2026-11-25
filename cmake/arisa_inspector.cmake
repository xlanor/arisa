if(NINTENDO_SWITCH AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(arisa_inspector_default OFF)
else()
    set(arisa_inspector_default ON)
endif()
option(ARISA_INSPECTOR "Build the arisa inspector library for apps that opt in"
    ${arisa_inspector_default})

if(ARISA_INSPECTOR AND TARGET arisa_runtime)
    add_library(arisa_inspector STATIC
        src/inspector/cost_ledger.cpp
        src/inspector/cost_panel.cpp
        src/inspector/focus_panel.cpp
        src/inspector/frame_history.cpp
        src/inspector/gpu_panel.cpp
        src/inspector/inspector.cpp
        src/inspector/inspector_model.cpp
        src/inspector/inspector_panel.cpp
        src/inspector/inspector_style.cpp
        src/inspector/inspector_view.cpp
        src/inspector/pick_panel.cpp
        src/inspector/timeline_panel.cpp
        src/inspector/token_editor.cpp
        src/inspector/token_row.cpp
        src/inspector/tokens_panel.cpp
        src/inspector/tree_panel.cpp)
    target_link_libraries(arisa_inspector
        PUBLIC arisa_runtime arisa_components arisa_controls arisa_composition
            arisa_appearance arisa_theme)
    target_compile_definitions(arisa_inspector INTERFACE ARISA_INSPECTOR=1)
    arisa_enable_strict_warnings(arisa_inspector)
endif()
