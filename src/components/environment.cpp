#include <arisa/components/environment.hpp>
#include <arisa/composition/accessibility_settings.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/composition/feedback_enablement.hpp>
#include <arisa/composition/focus_effect.hpp>
#include <arisa/composition/focus_environment.hpp>
#include <arisa/composition/hover_effect_environment.hpp>
#include <arisa/composition/reduce_motion.hpp>
#include <arisa/composition/safe_area.hpp>
#include <arisa/composition/scroll_environment.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/text_environment.hpp>
#include <arisa/composition/text_font.hpp>
#include <arisa/composition/tint.hpp>

namespace arisa::components {

const composition::EnvironmentKey<arisa::theme::Theme> Environment::theme{
    "theme", arisa::theme::Theme::defaults()};

const composition::EnvironmentKey<std::optional<arisa::graphics::ShapeStyle>>&
    Environment::tint = composition::Tint::key;

const composition::EnvironmentKey<std::optional<arisa::theme::ColorScheme>>
    Environment::colour_scheme{"colour_scheme", std::nullopt};

const composition::EnvironmentKey<text::Locale> Environment::locale{
    "locale", text::Locale::undetermined()};

const composition::EnvironmentKey<LayoutDirection>
    Environment::layout_direction{"layout_direction",
                                  LayoutDirection::left_to_right};

const composition::EnvironmentKey<std::optional<arisa::theme::ColorScheme>>&
    Environment::color_scheme = Environment::colour_scheme;

const composition::EnvironmentKey<bool>& Environment::reduce_motion =
    composition::ReduceMotion::key;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_reduce_motion = composition::ReduceMotion::key;

const composition::EnvironmentKey<bool>
    Environment::accessibility_reduce_transparency{
        "accessibility_reduce_transparency", false};

const composition::EnvironmentKey<arisa::theme::ColorSchemeContrast>
    Environment::color_scheme_contrast{
        "color_scheme_contrast", arisa::theme::ColorSchemeContrast::standard};

const composition::EnvironmentKey<arisa::theme::DynamicTypeSize>
    Environment::dynamic_type_size{"dynamic_type_size",
                                   arisa::theme::DynamicTypeSize::large};

const composition::EnvironmentKey<arisa::theme::DynamicTypeRange>
    Environment::dynamic_type_range{"dynamic_type_range",
                                    arisa::theme::DynamicTypeRange::all()};

const composition::EnvironmentKey<composition::InputModality>
    Environment::input_modality{"input_modality",
                                composition::InputModality::controller};

const composition::EnvironmentKey<std::vector<composition::CommandHint>>
    Environment::command_hints{"command_hints", {}};

const composition::EnvironmentKey<bool> Environment::controller_glyphs{
    "controller_glyphs", false};

const composition::EnvironmentKey<composition::DismissAction>
    Environment::dismiss{"dismiss", composition::DismissAction{}};

const composition::EnvironmentKey<bool> Environment::is_presented{
    "is_presented", false};

const composition::EnvironmentKey<provider::ScenePhase>
    Environment::scene_phase{"scene_phase", provider::ScenePhase::active};

const composition::EnvironmentKey<bool>& Environment::is_searching =
    composition::SearchEnvironment::is_searching;
const composition::EnvironmentKey<composition::DismissSearchAction>&
    Environment::dismiss_search =
        composition::SearchEnvironment::dismiss_search;
const composition::EnvironmentKey<bool>& Environment::is_enabled =
    composition::Enablement::is_enabled;

const composition::EnvironmentKey<bool>& Environment::is_scroll_enabled =
    composition::ScrollEnvironment::is_scroll_enabled;
const composition::EnvironmentKey<composition::ScrollIndicatorVisibility>&
    Environment::horizontal_scroll_indicator_visibility =
        composition::ScrollEnvironment::horizontal_scroll_indicator_visibility;
const composition::EnvironmentKey<composition::ScrollIndicatorVisibility>&
    Environment::vertical_scroll_indicator_visibility =
        composition::ScrollEnvironment::vertical_scroll_indicator_visibility;
const composition::EnvironmentKey<composition::ScrollBounceBehavior>&
    Environment::horizontal_scroll_bounce_behavior =
        composition::ScrollEnvironment::horizontal_scroll_bounce_behavior;
const composition::EnvironmentKey<composition::ScrollBounceBehavior>&
    Environment::vertical_scroll_bounce_behavior =
        composition::ScrollEnvironment::vertical_scroll_bounce_behavior;
const composition::EnvironmentKey<composition::ScrollDismissesKeyboardMode>&
    Environment::scroll_dismisses_keyboard_mode =
        composition::ScrollEnvironment::scroll_dismisses_keyboard_mode;

const composition::EnvironmentKey<bool>& Environment::is_focus_effect_enabled =
    composition::FocusEffect::is_focus_effect_enabled;
const composition::EnvironmentKey<bool>& Environment::is_hover_effect_enabled =
    composition::HoverEffectEnvironment::is_hover_effect_enabled;

const composition::EnvironmentKey<bool>&
    Environment::is_sensory_feedback_enabled =
        composition::FeedbackEnablement::is_sensory_feedback_enabled;

const composition::EnvironmentKey<bool>&
    Environment::is_system_feedback_enabled =
        composition::FeedbackEnablement::is_system_feedback_enabled;

const composition::EnvironmentKey<provider::HapticStrength>&
    Environment::haptic_strength =
        composition::FeedbackEnablement::haptic_strength;
const composition::EnvironmentKey<theme::Edges>& Environment::safe_area_insets =
    composition::SafeArea::insets;

const composition::EnvironmentKey<std::optional<arisa::theme::Font>>&
    Environment::font = composition::TextFont::font;

const composition::EnvironmentKey<std::optional<runtime::TextInput>>
    Environment::text_input{"text_input", std::nullopt};

const composition::EnvironmentKey<std::optional<ImageLibrary>>
    Environment::image_library{"image_library", std::nullopt};
const composition::EnvironmentKey<std::optional<ImageCache>>
    Environment::image_cache{"image_cache", std::nullopt};

const composition::EnvironmentKey<SystemStatus> Environment::system_status{
    "system_status", SystemStatus{}};

const composition::EnvironmentKey<EditMode> Environment::edit_mode{
    "edit_mode", EditMode::inactive};

const composition::EnvironmentKey<std::optional<appearance::AppearanceModel>>
    Environment::appearance{"appearance", std::nullopt};

const composition::EnvironmentKey<composition::NavigationSplitViewVisibility>
    Environment::sidebar_visibility{
        "sidebar_visibility", composition::NavigationSplitViewVisibility::all};

const composition::EnvironmentKey<provider::DisplayMode>
    Environment::display_mode{"display_mode", provider::DisplayMode::desktop};

const composition::EnvironmentKey<float> Environment::display_scale{
    "display_scale", 1.0F};

const composition::EnvironmentKey<float> Environment::pixel_length{
    "pixel_length", 1.0F};

const composition::EnvironmentKey<UserInterfaceSizeClass>
    Environment::horizontal_size_class{"horizontal_size_class",
                                       UserInterfaceSizeClass::regular};

const composition::EnvironmentKey<UserInterfaceSizeClass>
    Environment::vertical_size_class{"vertical_size_class",
                                     UserInterfaceSizeClass::regular};

const composition::EnvironmentKey<composition::InterfaceOrientation>
    Environment::interface_orientation{
        "interface_orientation",
        composition::InterfaceOrientation::landscape_left};

const composition::EnvironmentKey<composition::ResetFocusAction>
    Environment::reset_focus{"reset_focus", composition::ResetFocusAction{}};

const composition::EnvironmentKey<std::optional<composition::Axis>>
    Environment::split_arrangement_axis{"split_arrangement_axis", std::nullopt};

const composition::EnvironmentKey<int> Environment::overlay_arrangement_z_index{
    "overlay_arrangement_z_index", 0};

const composition::EnvironmentKey<std::optional<composition::Pasteboard>>&
    Environment::pasteboard = composition::Pasteboard::general;

const composition::EnvironmentKey<OpenURLAction> Environment::open_url{
    "open_url", OpenURLAction{}, &OpenURLAction::merged};

const composition::EnvironmentKey<std::optional<RefreshAction>>
    Environment::refresh{"refresh", std::nullopt};

const composition::EnvironmentKey<ControlSize> Environment::control_size{
    "control_size", ControlSize::regular};
const composition::EnvironmentKey<ButtonBorderShape>
    Environment::button_border_shape{"button_border_shape",
                                     ButtonBorderShape::automatic()};
const composition::EnvironmentKey<ButtonRepeatBehavior>
    Environment::button_repeat_behavior{"button_repeat_behavior",
                                        ButtonRepeatBehavior::automatic};
const composition::EnvironmentKey<ButtonSizing> Environment::button_sizing{
    "button_sizing", ButtonSizing::automatic};
const composition::EnvironmentKey<std::optional<EditModeBinding>>
    Environment::edit_mode_binding{"edit_mode_binding", std::nullopt};
const composition::EnvironmentKey<std::optional<RenameAction>>
    Environment::rename_action{"rename_action", std::nullopt};

const composition::EnvironmentKey<bool>& Environment::is_focused =
    composition::FocusEnvironment::is_focused;

const composition::EnvironmentKey<Calendar> Environment::calendar{"calendar",
                                                                  Calendar{}};

const composition::EnvironmentKey<TimeZone> Environment::time_zone{
    "time_zone", TimeZone::gmt()};

const composition::EnvironmentKey<bool> Environment::is_luminance_reduced{
    "is_luminance_reduced", false};

const composition::EnvironmentKey<std::optional<composition::UserDefaults>>&
    Environment::default_app_storage = composition::UserDefaults::app_storage;

const composition::EnvironmentKey<std::optional<text::StringCatalog>>&
    Environment::string_catalog = composition::TextEnvironment::string_catalog;

const composition::EnvironmentKey<composition::LegibilityWeight>&
    Environment::legibility_weight =
        composition::TextEnvironment::legibility_weight;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_differentiate_without_color =
        composition::AccessibilitySettings::differentiate_without_color;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_show_button_shapes =
        composition::AccessibilitySettings::show_button_shapes;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_dim_flashing_lights =
        composition::AccessibilitySettings::dim_flashing_lights;

const composition::EnvironmentKey<bool>& Environment::accessibility_enabled =
    composition::AccessibilitySettings::enabled;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_voice_over_enabled =
        composition::AccessibilitySettings::voice_over_enabled;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_switch_control_enabled =
        composition::AccessibilitySettings::switch_control_enabled;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_invert_colors =
        composition::AccessibilitySettings::invert_colors;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_play_animated_images =
        composition::AccessibilitySettings::play_animated_images;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_prefers_cross_fade_transitions =
        composition::AccessibilitySettings::prefers_cross_fade_transitions;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_reduce_highlighting_effects =
        composition::AccessibilitySettings::reduce_highlighting_effects;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_show_borders =
        composition::AccessibilitySettings::show_borders;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_assistive_access_enabled =
        composition::AccessibilitySettings::assistive_access_enabled;

const composition::EnvironmentKey<bool>&
    Environment::accessibility_large_content_viewer_enabled =
        composition::AccessibilitySettings::large_content_viewer_enabled;

const composition::EnvironmentKey<composition::AccessibilityTechnologies>&
    Environment::accessibility_technologies =
        composition::AccessibilitySettings::technologies;

const composition::EnvironmentKey<std::optional<FileStorage>>
    Environment::file_storage{"file_storage", std::nullopt};

const composition::EnvironmentKey<std::optional<SystemApplets>>
    Environment::system_applets{"system_applets", std::nullopt};

const composition::EnvironmentKey<std::optional<OpenURLInbox>>
    Environment::open_url_inbox{"open_url_inbox", std::nullopt};

const composition::EnvironmentKey<bool> Environment::is_scene_captured{
    "is_scene_captured", false};

const composition::EnvironmentKey<bool>
    Environment::is_user_authentication_enabled{
        "is_user_authentication_enabled", false};

const composition::EnvironmentKey<bool>
    Environment::system_prefers_reduced_resource_usage{
        "system_prefers_reduced_resource_usage", false};

const composition::EnvironmentKey<std::optional<composition::UndoManager>>
    Environment::undo_manager{"undo_manager", std::nullopt};

const composition::EnvironmentKey<std::optional<DocumentConfiguration>>
    Environment::document_configuration{"document_configuration", std::nullopt};

const composition::EnvironmentKey<NewDocumentAction> Environment::new_document{
    "new_document", NewDocumentAction{}};

const composition::EnvironmentKey<OpenDocumentAction>
    Environment::open_document{"open_document", OpenDocumentAction{}};

const composition::EnvironmentKey<composition::Visibility>
    Environment::persistent_system_overlays{"persistent_system_overlays",
                                            composition::Visibility::automatic};

const composition::EnvironmentKey<std::optional<std::string>>
    Environment::document_launch_title{"document_launch_title", std::nullopt};

const composition::EnvironmentKey<std::optional<std::string>>
    Environment::document_launch_subtitle{"document_launch_subtitle",
                                          std::nullopt};

auto Environment::font_resolution_context(composition::Context& context)
    -> theme::Font::Context {
  return context.environment(theme).font_context(
      context.environment(composition::TextEnvironment::legibility_weight) ==
      composition::LegibilityWeight::bold);
}

}

namespace arisa::composition {

auto Element::control_size(components::ControlSize size) && -> Element {
  return std::move(*this).environment(components::Environment::control_size,
                                      size);
}

auto Element::button_sizing(components::ButtonSizing sizing) && -> Element {
  return std::move(*this).environment(components::Environment::button_sizing,
                                      sizing);
}

auto Element::document_launch_title(std::string title) && -> Element {
  return std::move(*this).environment(
      components::Environment::document_launch_title,
      std::optional<std::string>{std::move(title)});
}

auto Element::document_launch_subtitle(std::string subtitle) && -> Element {
  return std::move(*this).environment(
      components::Environment::document_launch_subtitle,
      std::optional<std::string>{std::move(subtitle)});
}

}
