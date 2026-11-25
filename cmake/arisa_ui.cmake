add_library(arisa_sml INTERFACE)
target_include_directories(arisa_sml SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sml/include")
target_compile_definitions(arisa_sml INTERFACE BOOST_SML_DISABLE_EXCEPTIONS=true)

add_library(arisa_theme STATIC
    src/theme/dynamic_type.cpp
    src/theme/hsb.cpp
    src/theme/colour_mix.cpp
    src/theme/oklab.cpp
    src/theme/oklch.cpp
    src/theme/semantic_palette.cpp
    src/theme/theme.cpp
    src/theme/colour_value.cpp
    src/theme/typography.cpp)
target_link_libraries(arisa_theme PUBLIC arisa_support)
arisa_enable_strict_warnings(arisa_theme)

add_library(arisa_motion STATIC
    src/motion/animated_value.cpp
    src/motion/animation.cpp
    src/motion/animation_completion.cpp
    src/motion/animation_track.cpp
    src/motion/base_animation.cpp
    src/motion/completion_queue.cpp
    src/motion/custom_animation.cpp
    src/motion/keyframe_sequence.cpp
    src/motion/motion_path.cpp
    src/motion/presentation.cpp
    src/motion/spring.cpp
    src/motion/transaction.cpp
    src/motion/transaction_stack.cpp
    src/motion/transition.cpp
    src/motion/unit_curve.cpp)
target_link_libraries(arisa_motion PUBLIC arisa_support arisa_theme)
arisa_enable_strict_warnings(arisa_motion)

add_library(arisa_composition STATIC
    src/composition/accessibility.cpp
    src/composition/accessibility_chart_descriptor.cpp
    src/composition/accessibility_settings.cpp
    src/composition/accessibility_speech.cpp
    src/composition/accessibility_system_rotor.cpp
    src/composition/accessibility_text.cpp
    src/composition/animatable_data.cpp
    src/composition/command.cpp
    src/composition/container_background_placement.cpp
    src/composition/context.cpp
    src/composition/control_environment.cpp
    src/composition/decoration.cpp
    src/composition/drag_source.cpp
    src/composition/drop_delegate.cpp
    src/composition/keyboard_environment.cpp
    src/composition/modifier_key_alternate.cpp
    src/composition/element.cpp
    src/composition/element_content.cpp
    src/composition/reduce_motion.cpp
    src/composition/symbol_effect.cpp
    src/composition/element_key.cpp
    src/composition/enablement.cpp
    src/composition/feedback_enablement.cpp
    src/composition/file_url.cpp
    src/composition/list_environment.cpp
    src/composition/focus_effect.cpp
    src/composition/custom_hover_effect.cpp
    src/composition/hover_effect_environment.cpp
    src/composition/hover_effect_group.cpp
    src/composition/pointer_style.cpp
    src/composition/focus_follow.cpp
    src/composition/image_environment.cpp
    src/composition/safe_area.cpp
    src/composition/bar_placement.cpp
    src/composition/error_boundary.cpp
    src/composition/exclusive_scopes.cpp
    src/composition/layer.cpp
    src/composition/matched_transition_source_configuration.cpp
    src/composition/node.cpp
    src/composition/node_anchor.cpp
    src/composition/node_task.cpp
    src/composition/observable_core.cpp
    src/composition/paint_order.cpp
    src/composition/paging_scroll_target_behavior.cpp
    src/composition/pasteboard.cpp
    src/composition/placeholder_content_view.cpp
    src/composition/preferred_color_scheme.cpp
    src/composition/spring_loading_behavior.cpp
    src/composition/system_gesture_deferral.cpp
    src/composition/position.cpp
    src/composition/presentation_background.cpp
    src/composition/redaction.cpp
    src/composition/reconciler.cpp
    src/composition/retained.cpp
    src/composition/scroll_environment.cpp
    src/composition/search_environment.cpp
    src/composition/scroll_view_proxy.cpp
    src/composition/submission.cpp
    src/composition/text_environment.cpp
    src/composition/text_input_environment.cpp
    src/composition/text_font.cpp
    src/composition/text_input_configuration.cpp
    src/composition/tint.cpp
    src/composition/background_environment.cpp
    src/composition/tree.cpp
    src/composition/view_aligned_scroll_target_behavior.cpp
    src/composition/view_dimensions.cpp
    src/composition/focus_environment.cpp
    src/composition/navigation_chrome.cpp
    src/composition/navigation_declarations.cpp
    src/composition/toolbar_customization.cpp
    src/composition/toolbar_environment.cpp
    src/composition/presentation_sizing.cpp
    src/composition/undo_manager.cpp
    src/composition/persistent_system_overlays.cpp
    src/composition/scene_preferences.cpp
    src/composition/background_task.cpp
    src/composition/background_task_scheduler.cpp
    src/composition/view_modifier.cpp
    src/bridge/toml/user_defaults.cpp)
target_include_directories(arisa_composition PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(arisa_composition
    PUBLIC arisa_support arisa_async arisa_theme arisa_motion arisa_provider
        arisa_graphics arisa_text
    PRIVATE arisa_sml arisa_toml_vendor)
arisa_enable_strict_warnings(arisa_composition)

add_library(arisa_toml_vendor STATIC
    src/bridge/toml/toml_implementation.cpp)
target_include_directories(arisa_toml_vendor SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tomlplusplus")
target_compile_definitions(arisa_toml_vendor PUBLIC
    TOML_HEADER_ONLY=0
    TOML_EXCEPTIONS=0
    TOML_ENABLE_WINDOWS_COMPAT=0
    TOML_ENABLE_UNRELEASED_FEATURES=0
    TOML_FLOAT_CHARCONV=1)
target_compile_features(arisa_toml_vendor PUBLIC cxx_std_20)
arisa_disable_exceptions_and_rtti(arisa_toml_vendor)
if(NINTENDO_SWITCH)
    target_compile_options(arisa_toml_vendor PRIVATE -Os)
endif()

add_library(arisa_appearance STATIC
    src/appearance/appearance_model.cpp
    src/appearance/appearance_policy.cpp
    src/appearance/appearance_settings.cpp
    src/appearance/theme_error.cpp
    src/appearance/theme_edits.cpp
    src/appearance/theme_library.cpp
    src/appearance/theme_token.cpp
    src/appearance/theme_vocabulary.cpp
    src/appearance/theme_watcher.cpp
    src/bridge/toml/appearance_file.cpp
    src/bridge/toml/theme_file.cpp)
target_link_libraries(arisa_appearance
    PUBLIC arisa_theme arisa_composition arisa_provider
    PRIVATE arisa_toml_vendor)
arisa_enable_strict_warnings(arisa_appearance)

add_library(arisa_text_input STATIC
    src/runtime/text_input.cpp
    src/runtime/text_input_error.cpp)
target_link_libraries(arisa_text_input PUBLIC arisa_async arisa_provider)
arisa_enable_strict_warnings(arisa_text_input)

add_library(arisa_graphics STATIC
    src/graphics/any_shape.cpp
    src/graphics/colour_matrix.cpp
    src/graphics/combined_shape.cpp
    src/graphics/gradient.cpp
    src/graphics/offset_shape.cpp
    src/graphics/path.cpp
    src/graphics/path_clipper.cpp
    src/graphics/rotated_shape.cpp
    src/graphics/rotation_3d.cpp
    src/graphics/scaled_shape.cpp
    src/graphics/shader.cpp
    src/graphics/shape_style.cpp
    src/graphics/sized_shape.cpp
    src/graphics/style_hierarchy.cpp
    src/graphics/transformed_shape.cpp)
target_sources(arisa_graphics PRIVATE "${ARISA_SHADER_REGISTRY}")
target_include_directories(arisa_graphics PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(arisa_graphics PUBLIC arisa_support arisa_theme arisa_motion
    PRIVATE arisa_libtess2_vendor)
arisa_enable_strict_warnings(arisa_graphics)

add_library(arisa_nanovg_vendor STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanovg/nanovg_path.c")
target_include_directories(arisa_nanovg_vendor SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanovg")
if(NINTENDO_SWITCH)
    target_compile_options(arisa_nanovg_vendor PRIVATE -O2)
endif()

file(GLOB arisa_libtess2_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtess2/Source/*.c")
add_library(arisa_libtess2_vendor STATIC ${arisa_libtess2_sources})
target_include_directories(arisa_libtess2_vendor SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtess2/Include")
if(NINTENDO_SWITCH)
    target_compile_options(arisa_libtess2_vendor PRIVATE -O2)
endif()

add_library(arisa_components STATIC
    src/components/attributed_string.cpp
    src/components/inline_markdown.cpp
    src/components/any_layout.cpp
    src/components/grid_layout.cpp
    src/components/hstack_layout.cpp
    src/components/stack_distribution.cpp
    src/components/vstack_layout.cpp
    src/components/zstack_layout.cpp
    src/components/async_image.cpp
    src/components/timeline_schedule.cpp
    src/components/timeline_view.cpp
    src/components/timed_wake.cpp
    src/components/async_image_phase.cpp
    src/components/button.cpp
    src/components/button_border_shape.cpp
    src/components/button_style.cpp
    src/components/accessibility_tree.cpp
    src/components/arrangement_view.cpp
    src/components/arrangement_view_style.cpp
    src/components/calendar.cpp
    src/components/file_storage.cpp
    src/components/file_wrapper.cpp
    src/components/document_error.cpp
    src/components/document_content_types.cpp
    src/components/open_url_inbox.cpp
    src/components/column_resizer.cpp
    src/components/content_unavailable_view.cpp
    src/components/control_metrics.cpp
    src/components/divider.cpp
    src/components/environment.cpp
    src/components/for_each.cpp
    src/components/glass_effect_container.cpp
    src/components/glass_panel.cpp
    src/components/qr_code.cpp
    src/components/canvas.cpp
    src/components/geometry_reader.cpp
    src/components/graphics_context.cpp
    src/components/glyph.cpp
    src/components/system_status.cpp
    src/components/system_symbols.cpp
    src/components/url_session.cpp
    src/components/share_destination.cpp
    src/components/grid.cpp
    src/components/grid_item.cpp
    src/components/grid_row.cpp
    src/components/grid_section.cpp
    src/components/group.cpp
    src/components/hint_bar.cpp
    src/components/hstack.cpp
    src/components/image.cpp
    src/components/image_cache.cpp
    src/components/image_library.cpp
    src/components/image_source.cpp
    src/components/index_set.cpp
    src/components/label_column.cpp
    src/components/labeled_content.cpp
    src/components/labeled_content_style.cpp
    src/components/label.cpp
    src/components/label_style.cpp
    src/components/lazy_grid.cpp
    src/components/lazy_hgrid.cpp
    src/components/lazy_hstack.cpp
    src/components/lazy_stack.cpp
    src/components/lazy_vgrid.cpp
    src/components/lazy_vstack.cpp
    src/components/link.cpp
    src/components/mesh_gradient.cpp
    src/components/open_url_action.cpp
    src/components/pinned_view.cpp
    src/components/primitive_button_style.cpp
    src/components/reorder_cell.cpp
    src/components/safe_area_inset.cpp
    src/components/scroll_view.cpp
    src/components/scroll_view_reader.cpp
    src/components/shape_view.cpp
    src/components/spacer.cpp
    src/components/section_collection.cpp
    src/components/subviews_collection.cpp
    src/components/subviews_collection_slice.cpp
    src/components/surface_view.cpp
    src/components/status/battery.cpp
    src/components/status/clock.cpp
    src/components/status/wireless.cpp
    src/components/attributed_text_formatting.cpp
    src/components/attributed_text_selection.cpp
    src/components/text.cpp
    src/components/text_glyphs.cpp
    src/components/text_layout.cpp
    src/components/text_layout_key.cpp
    src/components/text_proxy.cpp
    src/components/text_renderer.cpp
    src/components/text_producer.cpp
    src/components/system_format_style.cpp
    src/components/date_interval.cpp
    src/components/time_zone.cpp
    src/components/view_that_fits.cpp
    src/components/vstack.cpp
    src/components/zstack.cpp)
target_link_libraries(arisa_components
    PUBLIC arisa_composition arisa_theme arisa_text arisa_text_input
        arisa_appearance arisa_graphics)
target_include_directories(arisa_components PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
arisa_enable_strict_warnings(arisa_components)

add_library(arisa_interaction STATIC
    src/interaction/drag_session.cpp
    src/interaction/focus_engine.cpp
    src/interaction/gesture_arena.cpp
    src/interaction/input_router.cpp
    src/interaction/invocation.cpp
    src/interaction/scroll_coordinator.cpp
    src/interaction/spatial_search.cpp
    src/interaction/stick_navigation.cpp
    src/interaction/velocity_tracker.cpp)
target_include_directories(arisa_interaction PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(arisa_interaction
    PUBLIC arisa_composition arisa_provider arisa_sml)
arisa_enable_strict_warnings(arisa_interaction)

add_library(arisa_presentation STATIC
    src/presentation/navigation_link.cpp
    src/presentation/navigation_stack.cpp
    src/presentation/help.cpp
    src/presentation/inspector.cpp
    src/presentation/menu_panel.cpp
    src/presentation/presented_destination.cpp
    src/presentation/toolbar_layout.cpp
    src/presentation/modal_presentation.cpp
    src/presentation/presentation_layer.cpp)
target_link_libraries(arisa_presentation PUBLIC arisa_components arisa_composition)
arisa_enable_strict_warnings(arisa_presentation)

add_library(arisa_controls STATIC
    src/controls/calendar_month.cpp
    src/controls/clipboard_commands.cpp
    src/controls/color_picker.cpp
    src/controls/context_menu.cpp
    src/controls/date_format.cpp
    src/controls/date_picker.cpp
    src/controls/date_picker_style.cpp
    src/controls/edit_button.cpp
    src/controls/gauge.cpp
    src/controls/gauge_style.cpp
    src/controls/menu.cpp
    src/controls/menu_style.cpp
    src/controls/multi_date_picker.cpp
    src/controls/paste_button.cpp
    src/controls/picker.cpp
    src/controls/picker_style.cpp
    src/controls/progress_view.cpp
    src/controls/progress_view_style.cpp
    src/controls/refreshable.cpp
    src/controls/rename_button.cpp
    src/controls/search_field.cpp
    src/controls/searchable.cpp
    src/controls/secure_field.cpp
    src/controls/slider.cpp
    src/controls/slider_tick.cpp
    src/controls/slider_tick_content.cpp
    src/controls/palette_selection_effect.cpp
    src/controls/text_field_link.cpp
    src/controls/share_link.cpp
    src/controls/stepper.cpp
    src/controls/text_editor.cpp
    src/controls/text_editor_style.cpp
    src/controls/text_field.cpp
    src/controls/text_field_style.cpp
    src/controls/text_editing.cpp
    src/controls/text_editing_session.cpp
    src/controls/text_selection_host.cpp
    src/controls/text_input_border_shape.cpp
    src/controls/text_input_suggestions.cpp
    src/controls/find_navigator.cpp
    src/controls/toggle.cpp
    src/controls/toggle_style.cpp)
target_link_libraries(arisa_controls
    PUBLIC arisa_components arisa_composition arisa_presentation)
arisa_enable_strict_warnings(arisa_controls)

add_library(arisa_structure STATIC
    src/structure/appearance_settings_section.cpp
    src/structure/cell_style.cpp
    src/structure/detail_presenter.cpp
    src/structure/disclosure_group.cpp
    src/structure/file_browser.cpp
    src/structure/assistive_access.cpp
    src/structure/default_document_group_launch_actions.cpp
    src/structure/document_group.cpp
    src/structure/document_group_launch_scene.cpp
    src/structure/document_launch_view.cpp
    src/structure/new_document_button.cpp
    src/structure/disclosure_group_style.cpp
    src/structure/dock_bar.cpp
    src/structure/disclosure_row.cpp
    src/structure/callout_board.cpp
    src/structure/digit_keypad.cpp
    src/structure/pin_field.cpp
    src/structure/tile_row.cpp
    src/structure/control_group.cpp
    src/structure/control_group_style.cpp
    src/structure/form.cpp
    src/structure/form_style.cpp
    src/structure/group_box.cpp
    src/structure/group_box_style.cpp
    src/structure/haptic_strength_slider.cpp
    src/structure/list.cpp
    src/structure/list_cell.cpp
    src/structure/list_style.cpp
    src/structure/navigation_split_view.cpp
    src/structure/row_selection.cpp
    src/structure/scaffold.cpp
    src/structure/section.cpp
    src/structure/section_index.cpp
    src/structure/sidebar_selection.cpp
    src/structure/sidebar_toggle.cpp
    src/structure/swipe_row.cpp
    src/structure/navigation_view.cpp
    src/structure/tab.cpp
    src/structure/tab_view.cpp
    src/structure/tab_view_customization.cpp
    src/structure/tab_view_style.cpp
    src/structure/table.cpp
    src/structure/table_column_customization.cpp
    src/structure/table_style.cpp)
target_link_libraries(arisa_structure
    PUBLIC arisa_components arisa_composition arisa_presentation arisa_controls)
arisa_enable_strict_warnings(arisa_structure)

add_library(arisa_markup INTERFACE)
target_link_libraries(arisa_markup INTERFACE
    arisa_components arisa_presentation arisa_controls arisa_structure)

file(GLOB_RECURSE arisa_yoga_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/yoga/yoga/*.cpp")
add_library(arisa_yoga_vendor STATIC ${arisa_yoga_sources})
target_include_directories(arisa_yoga_vendor SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/yoga")
target_compile_features(arisa_yoga_vendor PUBLIC cxx_std_20)
arisa_disable_exceptions_and_rtti(arisa_yoga_vendor)
if(NINTENDO_SWITCH)
    target_compile_options(arisa_yoga_vendor PRIVATE -O2)
endif()

add_library(arisa_layout STATIC
    src/bridge/yoga/custom_arrangement.cpp
    src/bridge/yoga/grid_arrangement.cpp
    src/bridge/yoga/yoga_layout.cpp
    src/bridge/yoga/yoga_slot.cpp
    src/layout/layout_context.cpp
    src/layout/layout_error.cpp)
target_include_directories(arisa_layout PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(arisa_layout
    PUBLIC arisa_composition arisa_components arisa_text arisa_provider
    PRIVATE arisa_yoga_vendor)
arisa_enable_strict_warnings(arisa_layout)

add_library(arisa_rendering STATIC
    src/bridge/nanovg/path_tessellator.cpp
    src/rendering/compositor.cpp
    src/rendering/draw_list_cache.cpp
    src/rendering/glass_lens.cpp
    src/rendering/paint_error.cpp
    src/rendering/painter.cpp
    src/rendering/render_target_pool.cpp
    src/rendering/shape_painter.cpp)
target_include_directories(arisa_rendering PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(arisa_rendering
    PUBLIC arisa_composition arisa_components arisa_text arisa_theme arisa_provider
        arisa_graphics
    PRIVATE arisa_nanovg_vendor arisa_libtess2_vendor)
arisa_enable_strict_warnings(arisa_rendering)

add_library(arisa_runtime STATIC
    src/runtime/feedback_player.cpp
    src/runtime/frame_pacer.cpp
    src/runtime/render_thread.cpp
    src/runtime/application.cpp
    src/runtime/application_error.cpp
    src/runtime/dispatch_queue.cpp
    src/runtime/dispatcher.cpp
    src/runtime/drag_preview.cpp
    src/runtime/frame_breakdown.cpp
    src/runtime/input_gate.cpp
    src/runtime/button_capture.cpp
    src/runtime/introspection.cpp
    src/runtime/benchmark_host.cpp
    src/bridge/toml/view_debug.cpp
    src/runtime/screen_reader.cpp
    src/runtime/overlays.cpp
    src/runtime/painter_renderer.cpp
    src/runtime/pointer_cursor.cpp
    src/runtime/root.cpp)
target_include_directories(arisa_runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(arisa_runtime
    PUBLIC arisa_composition arisa_provider arisa_async_threads arisa_layout
        arisa_rendering arisa_text arisa_interaction arisa_text_input
        arisa_appearance
    PRIVATE arisa_components arisa_sml arisa_toml_vendor)
arisa_enable_strict_warnings(arisa_runtime)
if(TARGET arisa_provider_headless)
    target_sources(arisa_runtime PRIVATE src/runtime/image_renderer.cpp)
    target_link_libraries(arisa_runtime PUBLIC arisa_provider_headless)
endif()
