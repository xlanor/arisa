#pragma once

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/components/alignment.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_repeat_behavior.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/calendar.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/document_configuration.hpp>
#include <arisa/components/edit_mode.hpp>
#include <arisa/components/edit_mode_binding.hpp>
#include <arisa/components/file_storage.hpp>
#include <arisa/components/image_cache.hpp>
#include <arisa/components/image_library.hpp>
#include <arisa/components/new_document_action.hpp>
#include <arisa/components/open_document_action.hpp>
#include <arisa/components/open_url_action.hpp>
#include <arisa/components/open_url_inbox.hpp>
#include <arisa/components/refresh_action.hpp>
#include <arisa/components/rename_action.hpp>
#include <arisa/components/system_applets.hpp>
#include <arisa/components/system_status.hpp>
#include <arisa/components/time_zone.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/composition/accessibility_settings.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/command_hint.hpp>
#include <arisa/composition/dismiss_action.hpp>
#include <arisa/composition/dismiss_search_action.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/input_modality.hpp>
#include <arisa/composition/interface_orientation.hpp>
#include <arisa/composition/legibility_weight.hpp>
#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/reset_focus_action.hpp>
#include <arisa/composition/scroll_bounce_behavior.hpp>
#include <arisa/composition/scroll_dismisses_keyboard_mode.hpp>
#include <arisa/composition/scroll_indicator_visibility.hpp>
#include <arisa/composition/undo_manager.hpp>
#include <arisa/composition/user_defaults.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/provider/haptics/haptic_strength.hpp>
#include <arisa/provider/lifecycle/provider.hpp>
#include <arisa/provider/surface/display_mode.hpp>
#include <arisa/runtime/text_input.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/string_catalog.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/color_scheme_contrast.hpp>
#include <arisa/theme/dynamic_type_size.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/theme.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::composition {

class Context;

}

namespace arisa::components {

class Environment {
 public:
  static const composition::EnvironmentKey<arisa::theme::Theme> theme;
  static const composition::EnvironmentKey<
      std::optional<arisa::graphics::ShapeStyle>>& tint;
  static const composition::EnvironmentKey<
      std::optional<arisa::theme::ColorScheme>>
      colour_scheme;
  static const composition::EnvironmentKey<text::Locale> locale;
  static const composition::EnvironmentKey<LayoutDirection> layout_direction;
  static const composition::EnvironmentKey<
      std::optional<arisa::theme::ColorScheme>>& color_scheme;
  static const composition::EnvironmentKey<bool>& reduce_motion;
  static const composition::EnvironmentKey<bool>& accessibility_reduce_motion;
  static const composition::EnvironmentKey<bool>
      accessibility_reduce_transparency;
  static const composition::EnvironmentKey<arisa::theme::ColorSchemeContrast>
      color_scheme_contrast;
  static const composition::EnvironmentKey<arisa::theme::DynamicTypeSize>
      dynamic_type_size;
  static const composition::EnvironmentKey<arisa::theme::DynamicTypeRange>
      dynamic_type_range;
  static const composition::EnvironmentKey<composition::InputModality>
      input_modality;
  static const composition::EnvironmentKey<
      std::vector<composition::CommandHint>>
      command_hints;
  static const composition::EnvironmentKey<bool> controller_glyphs;
  static const composition::EnvironmentKey<composition::DismissAction> dismiss;
  static const composition::EnvironmentKey<bool> is_presented;
  static const composition::EnvironmentKey<bool>& is_searching;
  static const composition::EnvironmentKey<composition::DismissSearchAction>&
      dismiss_search;
  static const composition::EnvironmentKey<provider::ScenePhase> scene_phase;
  static const composition::EnvironmentKey<bool>& is_enabled;
  static const composition::EnvironmentKey<bool>& is_focus_effect_enabled;
  static const composition::EnvironmentKey<bool>& is_hover_effect_enabled;
  static const composition::EnvironmentKey<bool>& is_sensory_feedback_enabled;
  static const composition::EnvironmentKey<bool>& is_system_feedback_enabled;
  static const composition::EnvironmentKey<provider::HapticStrength>&
      haptic_strength;
  static const composition::EnvironmentKey<theme::Edges>& safe_area_insets;
  static const composition::EnvironmentKey<std::optional<arisa::theme::Font>>&
      font;
  static const composition::EnvironmentKey<std::optional<runtime::TextInput>>
      text_input;
  static const composition::EnvironmentKey<std::optional<ImageLibrary>>
      image_library;
  static const composition::EnvironmentKey<std::optional<ImageCache>>
      image_cache;
  static const composition::EnvironmentKey<SystemStatus> system_status;
  static const composition::EnvironmentKey<EditMode> edit_mode;
  static const composition::EnvironmentKey<
      std::optional<appearance::AppearanceModel>>
      appearance;
  static const composition::EnvironmentKey<
      composition::NavigationSplitViewVisibility>
      sidebar_visibility;
  static const composition::EnvironmentKey<provider::DisplayMode> display_mode;
  static const composition::EnvironmentKey<float> display_scale;
  static const composition::EnvironmentKey<float> pixel_length;
  static const composition::EnvironmentKey<UserInterfaceSizeClass>
      horizontal_size_class;
  static const composition::EnvironmentKey<UserInterfaceSizeClass>
      vertical_size_class;
  static const composition::EnvironmentKey<composition::InterfaceOrientation>
      interface_orientation;
  static const composition::EnvironmentKey<OpenURLAction> open_url;
  static const composition::EnvironmentKey<std::optional<RefreshAction>>
      refresh;
  static const composition::EnvironmentKey<ControlSize> control_size;
  static const composition::EnvironmentKey<ButtonBorderShape>
      button_border_shape;
  static const composition::EnvironmentKey<ButtonRepeatBehavior>
      button_repeat_behavior;
  static const composition::EnvironmentKey<ButtonSizing> button_sizing;
  static const composition::EnvironmentKey<std::optional<EditModeBinding>>
      edit_mode_binding;
  static const composition::EnvironmentKey<std::optional<RenameAction>>
      rename_action;
  static const composition::EnvironmentKey<bool>& is_focused;
  static const composition::EnvironmentKey<Calendar> calendar;
  static const composition::EnvironmentKey<TimeZone> time_zone;
  static const composition::EnvironmentKey<bool> is_luminance_reduced;
  static const composition::EnvironmentKey<
      std::optional<composition::UserDefaults>>& default_app_storage;
  static const composition::EnvironmentKey<composition::ResetFocusAction>
      reset_focus;
  static const composition::EnvironmentKey<
      std::optional<composition::Pasteboard>>& pasteboard;
  static const composition::EnvironmentKey<std::optional<text::StringCatalog>>&
      string_catalog;
  static const composition::EnvironmentKey<composition::LegibilityWeight>&
      legibility_weight;
  static const composition::EnvironmentKey<bool>&
      accessibility_differentiate_without_color;
  static const composition::EnvironmentKey<bool>&
      accessibility_show_button_shapes;
  static const composition::EnvironmentKey<bool>&
      accessibility_dim_flashing_lights;
  static const composition::EnvironmentKey<bool>& accessibility_enabled;
  static const composition::EnvironmentKey<bool>&
      accessibility_voice_over_enabled;
  static const composition::EnvironmentKey<bool>&
      accessibility_switch_control_enabled;
  static const composition::EnvironmentKey<bool>& accessibility_invert_colors;
  static const composition::EnvironmentKey<bool>&
      accessibility_play_animated_images;
  static const composition::EnvironmentKey<bool>&
      accessibility_prefers_cross_fade_transitions;
  static const composition::EnvironmentKey<bool>&
      accessibility_reduce_highlighting_effects;
  static const composition::EnvironmentKey<bool>& accessibility_show_borders;
  static const composition::EnvironmentKey<bool>&
      accessibility_assistive_access_enabled;
  static const composition::EnvironmentKey<bool>&
      accessibility_large_content_viewer_enabled;
  static const composition::EnvironmentKey<
      composition::AccessibilityTechnologies>& accessibility_technologies;
  static const composition::EnvironmentKey<std::optional<FileStorage>>
      file_storage;
  static const composition::EnvironmentKey<std::optional<SystemApplets>>
      system_applets;
  static const composition::EnvironmentKey<bool>& is_scroll_enabled;
  static const composition::EnvironmentKey<
      composition::ScrollIndicatorVisibility>&
      horizontal_scroll_indicator_visibility;
  static const composition::EnvironmentKey<
      composition::ScrollIndicatorVisibility>&
      vertical_scroll_indicator_visibility;
  static const composition::EnvironmentKey<composition::ScrollBounceBehavior>&
      horizontal_scroll_bounce_behavior;
  static const composition::EnvironmentKey<composition::ScrollBounceBehavior>&
      vertical_scroll_bounce_behavior;
  static const composition::EnvironmentKey<
      composition::ScrollDismissesKeyboardMode>& scroll_dismisses_keyboard_mode;
  static const composition::EnvironmentKey<std::optional<composition::Axis>>
      split_arrangement_axis;
  static const composition::EnvironmentKey<int> overlay_arrangement_z_index;
  static const composition::EnvironmentKey<std::optional<OpenURLInbox>>
      open_url_inbox;
  static const composition::EnvironmentKey<bool> is_scene_captured;
  static const composition::EnvironmentKey<bool> is_user_authentication_enabled;
  static const composition::EnvironmentKey<bool>
      system_prefers_reduced_resource_usage;
  static const composition::EnvironmentKey<
      std::optional<composition::UndoManager>>
      undo_manager;
  static const composition::EnvironmentKey<std::optional<DocumentConfiguration>>
      document_configuration;
  static const composition::EnvironmentKey<NewDocumentAction> new_document;
  static const composition::EnvironmentKey<OpenDocumentAction> open_document;
  static const composition::EnvironmentKey<composition::Visibility>
      persistent_system_overlays;
  static const composition::EnvironmentKey<std::optional<std::string>>
      document_launch_title;
  static const composition::EnvironmentKey<std::optional<std::string>>
      document_launch_subtitle;

  static auto font_resolution_context(composition::Context& context)
      -> theme::Font::Context;
};

}

namespace arisa::composition {

template <class Theme>
auto Element::theme_override(Theme value) && -> Element {
  return std::move(*this).environment(components::Environment::theme,
                                      arisa::theme::Theme{std::move(value)});
}

template <class Shape>
auto Element::button_border_shape(Shape shape) && -> Element {
  return std::move(*this).environment(
      components::Environment::button_border_shape,
      components::ButtonBorderShape{shape});
}

template <class Behavior>
auto Element::button_repeat_behavior(Behavior behavior) && -> Element {
  return std::move(*this).environment(
      components::Environment::button_repeat_behavior,
      components::ButtonRepeatBehavior{behavior});
}

template <class Mode>
auto Element::edit_mode(Mode mode) && -> Element {
  const components::EditModeBinding bound{
      composition::Binding<components::EditMode>{std::move(mode)}};
  const components::EditMode current = bound.get();
  return std::move(*this)
      .environment(components::Environment::edit_mode, current)
      .environment(components::Environment::edit_mode_binding,
                   std::optional<components::EditModeBinding>{bound});
}

template <class Action>
auto Element::rename_action(Action action) && -> Element {
  return std::move(*this).environment(
      components::Environment::rename_action,
      std::optional<components::RenameAction>{
          components::RenameAction{std::function<void()>{std::move(action)}}});
}

template <class Size>
auto Element::dynamic_type_size(Size size) && -> Element {
  if constexpr (std::same_as<Size, arisa::theme::DynamicTypeRange>) {
    return std::move(*this).environment(
        components::Environment::dynamic_type_range, size);
  } else {
    return std::move(*this).environment(
        components::Environment::dynamic_type_size,
        arisa::theme::DynamicTypeSize{size});
  }
}

}
