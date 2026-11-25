#pragma once

#include <arisa/composition/background_task.hpp>
#include <arisa/composition/bar_placement.hpp>
#include <arisa/composition/task_options.hpp>
#include <arisa/composition/text_input_formatting_control_placement.hpp>
#include <arisa/composition/text_selection.hpp>
#include <arisa/composition/text_selection_affinity.hpp>
#include <arisa/composition/visibility.hpp>

#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/accessibility_action_category.hpp>
#include <arisa/composition/accessibility_action_kind.hpp>
#include <arisa/composition/accessibility_adjustment_direction.hpp>
#include <arisa/composition/accessibility_attachment.hpp>
#include <arisa/composition/accessibility_chart_descriptor.hpp>
#include <arisa/composition/accessibility_custom_content_key.hpp>
#include <arisa/composition/accessibility_direct_touch_options.hpp>
#include <arisa/composition/accessibility_focus_state.hpp>
#include <arisa/composition/accessibility_labeled_pair_role.hpp>
#include <arisa/composition/accessibility_rotor.hpp>
#include <arisa/composition/accessibility_rotor_entry.hpp>
#include <arisa/composition/accessibility_speech.hpp>
#include <arisa/composition/accessibility_text.hpp>
#include <arisa/composition/accessibility_zoom_gesture_action.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/alignment_guide.hpp>
#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/alternating_row_background_behavior.hpp>
#include <arisa/composition/anchor.hpp>
#include <arisa/composition/anchor_source.hpp>
#include <arisa/composition/any_scroll_target_behavior.hpp>
#include <arisa/composition/aspect_ratio.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/background_prominence.hpp>
#include <arisa/composition/badge_prominence.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/change_observer.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/container_background_placement.hpp>
#include <arisa/composition/container_relative_frame.hpp>
#include <arisa/composition/container_value_key.hpp>
#include <arisa/composition/content_margin_guide.hpp>
#include <arisa/composition/content_margin_placement.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/default_focus_evaluation_priority.hpp>
#include <arisa/composition/departure_hold.hpp>
#include <arisa/composition/dialog_severity.hpp>
#include <arisa/composition/drag_configuration.hpp>
#include <arisa/composition/drag_container.hpp>
#include <arisa/composition/drag_drop_previews_formation.hpp>
#include <arisa/composition/drag_selection.hpp>
#include <arisa/composition/drag_session.hpp>
#include <arisa/composition/drag_source.hpp>
#include <arisa/composition/drop_configuration.hpp>
#include <arisa/composition/drop_delegate.hpp>
#include <arisa/composition/drop_session.hpp>
#include <arisa/composition/drop_target.hpp>
#include <arisa/composition/editable_tokens.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/environment_object.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/environment_transform.hpp>
#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/feedback_origin.hpp>
#include <arisa/composition/feedback_trigger.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_interactions.hpp>
#include <arisa/composition/focus_request.hpp>
#include <arisa/composition/focus_scope_behavior.hpp>
#include <arisa/composition/focused_object.hpp>
#include <arisa/composition/focused_value_key.hpp>
#include <arisa/composition/geometry_observer.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_attachment.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/gesture_mask.hpp>
#include <arisa/composition/glass_effect.hpp>
#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/hover_phase.hpp>
#include <arisa/composition/image_attributes.hpp>
#include <arisa/composition/implicit_animation.hpp>
#include <arisa/composition/interactions.hpp>
#include <arisa/composition/key_equivalent.hpp>
#include <arisa/composition/key_press.hpp>
#include <arisa/composition/key_press_handler.hpp>
#include <arisa/composition/keyboard_environment.hpp>
#include <arisa/composition/keyboard_shortcut.hpp>
#include <arisa/composition/keyed_values.hpp>
#include <arisa/composition/keyframe_repeat.hpp>
#include <arisa/composition/layout_value_key.hpp>
#include <arisa/composition/list_item_tint.hpp>
#include <arisa/composition/list_section_spacing.hpp>
#include <arisa/composition/localized_error.hpp>
#include <arisa/composition/matched_geometry.hpp>
#include <arisa/composition/matched_transition_source_configuration.hpp>
#include <arisa/composition/menu_action_dismiss_behavior.hpp>
#include <arisa/composition/menu_order.hpp>
#include <arisa/composition/modifier_keys_handler.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/motion_declarations.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/navigation_declarations.hpp>
#include <arisa/composition/navigation_split_view_style.hpp>
#include <arisa/composition/node_hooks.hpp>
#include <arisa/composition/node_task.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/overlay_arrangement_edges.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/paging_scroll_target_behavior.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/pointer_style.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/preference.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/presentation_adaptation.hpp>
#include <arisa/composition/presentation_background_interaction.hpp>
#include <arisa/composition/presentation_content_interaction.hpp>
#include <arisa/composition/presentation_detent.hpp>
#include <arisa/composition/presentation_placement.hpp>
#include <arisa/composition/presentation_sizing.hpp>
#include <arisa/composition/presented_item.hpp>
#include <arisa/composition/prominence.hpp>
#include <arisa/composition/redaction_reasons.hpp>
#include <arisa/composition/reorder_difference.hpp>
#include <arisa/composition/reorder_item.hpp>
#include <arisa/composition/safe_area_regions.hpp>
#include <arisa/composition/scene_padding.hpp>
#include <arisa/composition/scroll_anchor_role.hpp>
#include <arisa/composition/scroll_bounce_behavior.hpp>
#include <arisa/composition/scroll_deceleration_rate.hpp>
#include <arisa/composition/scroll_dismisses_keyboard_mode.hpp>
#include <arisa/composition/scroll_effects.hpp>
#include <arisa/composition/scroll_geometry.hpp>
#include <arisa/composition/scroll_geometry_observer.hpp>
#include <arisa/composition/scroll_indicator_visibility.hpp>
#include <arisa/composition/scroll_input_behavior.hpp>
#include <arisa/composition/scroll_input_kind.hpp>
#include <arisa/composition/scroll_position.hpp>
#include <arisa/composition/scroll_target_behavior.hpp>
#include <arisa/composition/scroll_transition_configuration.hpp>
#include <arisa/composition/scrolling.hpp>
#include <arisa/composition/search_field_placement.hpp>
#include <arisa/composition/search_scope_activation.hpp>
#include <arisa/composition/search_suggestions_placement.hpp>
#include <arisa/composition/search_toolbar_behavior.hpp>
#include <arisa/composition/sensory_feedback.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/split_arrangement_sizing.hpp>
#include <arisa/composition/spring_loading_behavior.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/submission.hpp>
#include <arisa/composition/submit_triggers.hpp>
#include <arisa/composition/symbol_effect.hpp>
#include <arisa/composition/symbol_variants.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/composition/toast_options.hpp>
#include <arisa/composition/toolbar_minimization.hpp>
#include <arisa/composition/toolbar_placement.hpp>
#include <arisa/composition/toolbar_role.hpp>
#include <arisa/composition/toolbar_title_alignment.hpp>
#include <arisa/composition/toolbar_title_display_mode.hpp>
#include <arisa/composition/toolbar_vertical_behavior.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/transferable.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/composition/vertical_edge.hpp>
#include <arisa/composition/vertical_edges.hpp>
#include <arisa/composition/view_aligned_scroll_target_behavior.hpp>
#include <arisa/composition/view_dimensions.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/blend_mode.hpp>
#include <arisa/graphics/colour_matrix.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/rotation_3d.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/motion/matched_geometry_properties.hpp>
#include <arisa/motion/motion_path.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/text_input/text_input_traits.hpp>
#include <arisa/support/dismantle.hpp>
#include <arisa/support/sparse.hpp>
#include <arisa/text/typesetting_language.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::components {

enum class ButtonSizing : std::uint8_t;
enum class ControlSize : std::uint8_t;
enum class PanelRadius : std::uint8_t;

}

namespace arisa::presentation {

class PresentationLayer;

}

namespace arisa::composition {

class Component;
class PreferenceObserver;
class ToolbarItem;
class ToolbarItemGroup;
class ToolbarSpacer;
class UserDefaults;
struct ToolbarRemoval;
struct ListRow;

template <class T>
class ComponentContent;

template <class T>
concept ComponentValue =
    std::derived_from<T, Component> && std::move_constructible<T> &&
    std::equality_comparable<T>;

class SwipeHost;

class Element {
 public:
  using TransactionEdit = std::move_only_function<void(motion::Transaction&)>;

  template <class T>
    requires ComponentValue<std::remove_cvref_t<T>>
  Element(T&& component)
      : content_(std::make_unique<ComponentContent<std::remove_cvref_t<T>>>(
            std::forward<T>(component))) {}

  Element(Element&&) noexcept = default;

  auto operator=(Element&&) noexcept -> Element& = default;

  ~Element();

  auto id(ElementKey key) && -> Element;

  template <class T>
  auto environment(const EnvironmentKey<T>& key,
                   std::type_identity_t<T> value) && -> Element {
    if (std::ranges::find(environment_->overrides, key.identity(),
                          &EnvironmentOverride::key) !=
        environment_->overrides.end()) {
      return std::move(*this);
    }
    if (const auto merge = key.merge(); merge != nullptr) {
      environment_.edit().transforms.push_back(EnvironmentTransform::of(
          key, [merge, declared = std::move(value)](T& inherited) {
            inherited = merge(inherited, declared);
          }));
      return std::move(*this);
    }
    EnvironmentOverride set = EnvironmentOverride::of(key, std::move(value));
    for (auto inner = environment_->transforms.rbegin();
         inner != environment_->transforms.rend(); ++inner) {
      if (inner->key() == key.identity()) {
        set = inner->apply(&set.value());
      }
    }
    std::erase_if(environment_.edit().transforms,
                  [&key](const EnvironmentTransform& existing) {
                    return existing.key() == key.identity();
                  });
    environment_.edit().overrides.push_back(std::move(set));
    return std::move(*this);
  }

  template <class T>
  auto environment(Observable<T>& object) && -> Element {
    return std::move(*this).environment(
        environment_object_key<T>,
        std::optional<EnvironmentObject<T>>{EnvironmentObject<T>{object}});
  }

  template <class T>
  auto environment_object(Observable<T>& object) && -> Element {
    return std::move(*this).environment(object);
  }

  template <class T>
  auto declared_environment(const EnvironmentKey<T>& key) const noexcept
      -> std::optional<std::reference_wrapper<const T>> {
    for (const EnvironmentOverride& entry : environment_->overrides) {
      if (entry.key() == key.identity()) {
        return std::cref(
            static_cast<const EnvironmentOverride::Typed<T>&>(entry.value())
                .get());
      }
    }
    return std::nullopt;
  }

  template <class T, class Edit>
    requires std::invocable<const Edit&, T&>
  auto transform_environment(const EnvironmentKey<T>& key,
                             Edit edit) && -> Element {
    if (std::ranges::find(environment_->overrides, key.identity(),
                          &EnvironmentOverride::key) !=
        environment_->overrides.end()) {
      return std::move(*this);
    }
    environment_.edit().transforms.push_back(EnvironmentTransform::of(
        key, EnvironmentTransform::Edit<T>{std::move(edit)}));
    return std::move(*this);
  }

  template <class T>
  auto preference(const PreferenceKey<T>& key,
                  std::type_identity_t<T> value) && -> Element {
    environment_.edit().preferences.push_back(
        Preference::value(key, std::move(value)));
    return std::move(*this);
  }

  template <class T, class Edit>
    requires std::invocable<const Edit&, T&>
  auto transform_preference(const PreferenceKey<T>& key,
                            Edit edit) && -> Element {
    environment_.edit().preferences.push_back(Preference::transform(
        key, typename Preference::Transform<T>::Edit{std::move(edit)}));
    return std::move(*this);
  }

  template <class T, class Shape, class Resolve>
    requires std::convertible_to<
        std::invoke_result_t<const Resolve&, const Anchor<Shape>&>, T>
  auto anchor_preference(const PreferenceKey<T>& key,
                         AnchorSource<Shape> source,
                         Resolve resolve) && -> Element {
    environment_.edit().preferences.push_back(Preference::anchored(
        key, source,
        typename Preference::Anchored<T, Shape>::Resolve{std::move(resolve)}));
    return std::move(*this);
  }

  template <class T, class Shape, class Edit>
    requires std::invocable<const Edit&, T&, const Anchor<Shape>&>
  auto transform_anchor_preference(const PreferenceKey<T>& key,
                                   AnchorSource<Shape> source,
                                   Edit edit) && -> Element {
    environment_.edit().preferences.push_back(Preference::anchored_transform(
        key, source,
        typename Preference::AnchoredTransform<T, Shape>::Edit{
            std::move(edit)}));
    return std::move(*this);
  }

  template <class T, class Action>
    requires std::equality_comparable<T> && std::invocable<Action&, const T&>
  auto on_preference_change(const PreferenceKey<T>& key,
                            Action action) && -> Element;

  template <class T, class Build>
    requires std::equality_comparable<T>
  auto overlay_preference_value(const PreferenceKey<T>& key,
                                Build build) && -> Element;

  template <class T, class Build>
    requires std::equality_comparable<T>
  auto overlay_preference_value(const PreferenceKey<T>& key,
                                Alignment alignment, Build build) && -> Element;

  template <class T, class Build>
    requires std::equality_comparable<T>
  auto background_preference_value(const PreferenceKey<T>& key,
                                   Build build) && -> Element;

  template <class T, class Build>
    requires std::equality_comparable<T>
  auto background_preference_value(const PreferenceKey<T>& key,
                                   Alignment alignment,
                                   Build build) && -> Element;

  template <class Modifier>
  auto modifier(Modifier modifier) && -> Element;

  auto control_size(components::ControlSize size) && -> Element;

  template <class Shape>
  auto button_border_shape(Shape shape) && -> Element;

  template <class Behavior>
  auto button_repeat_behavior(Behavior behavior) && -> Element;

  auto button_sizing(components::ButtonSizing sizing) && -> Element;

  template <class Mode>
  auto edit_mode(Mode mode) && -> Element;

  template <class Action>
  auto rename_action(Action action) && -> Element;

  auto default_app_storage(UserDefaults store) && -> Element;

  template <class P, class Action>
  auto on_receive(P& publisher, Action action) && -> Element;

  template <class Action>
  auto on_open_url(Action action) && -> Element;

  template <class Action>
  auto background_task(BackgroundTask task, Action action) && -> Element;

  auto persistent_system_overlays(Visibility visibility) && -> Element;

  auto document_launch_title(std::string title) && -> Element;

  auto document_launch_subtitle(std::string subtitle) && -> Element;

  template <class Scene>
  auto document_group_launch_scene(Scene scene) && -> Element;

  template <class Scene>
  auto assistive_access(Scene scene) && -> Element;

  template <class Style>
  auto index_view_style(Style style) && -> Element;

  template <class Item>
  auto navigation_document(const Item& item) && -> Element;

  template <class Item, class Preview>
  auto navigation_document(const Item& item, Preview preview) && -> Element;

  auto on_appear(std::move_only_function<void()> action) && -> Element;

  auto on_disappear(std::move_only_function<void()> action) && -> Element;

  auto on_depart(
      std::move_only_function<void(DepartureHold)> action) && -> Element;

  auto task(NodeTask::Plain body) && -> Element;

  auto task(NodeTask::Fallible body) && -> Element;

  auto task(ElementKey key, NodeTask::Plain body) && -> Element;

  auto task(ElementKey key, NodeTask::Fallible body) && -> Element;

  auto task(TaskOptions options, NodeTask::Plain body) && -> Element;

  auto task(TaskOptions options, NodeTask::Fallible body) && -> Element;

  auto task(ElementKey key, TaskOptions options,
            NodeTask::Plain body) && -> Element;

  auto task(ElementKey key, TaskOptions options,
            NodeTask::Fallible body) && -> Element;

  auto padding() && -> Element;

  auto padding(theme::Space length) && -> Element;

  auto padding(theme::Insets insets) && -> Element;

  auto padding(theme::Edge edges, theme::Space length) && -> Element;

  auto scene_padding(theme::Edge edges = theme::Edge::all) && -> Element;

  auto scene_padding(ScenePadding padding,
                     theme::Edge edges = theme::Edge::all) && -> Element;

  auto margin(theme::Space length) && -> Element;

  auto margin(theme::Insets insets) && -> Element;

  auto flex_grow(float factor) && -> Element;

  auto flex_shrink(float factor) && -> Element;

  auto frame(FrameSpec frame) && -> Element;

  auto fixed_size() && -> Element;

  auto fixed_size(bool horizontal, bool vertical) && -> Element;

  auto layout_priority(double value) && -> Element;

  auto position(float x = 0.0F, float y = 0.0F) && -> Element;

  auto position(Point centre) && -> Element;

  auto z_index(double value) && -> Element;

  auto split_arrangement_layout_ratio(std::optional<float> ratio) && -> Element;

  auto split_arrangement_layout_ratio(
      SplitArrangementSizing::Ratios ratios) && -> Element;

  auto split_arrangement_layout_size(
      SplitArrangementSizing::Sizes sizes) && -> Element;

  auto split_arrangement_fixed_layout_size(bool horizontal = true,
                                           bool vertical = true) && -> Element;

  auto overlay_arrangement_edge(std::optional<VerticalEdge> edge) && -> Element;

  auto overlay_arrangement_edge(
      std::optional<HorizontalEdge> edge) && -> Element;

  template <class T>
    requires std::equality_comparable<T> && std::copy_constructible<T>
  auto layout_value(const LayoutValueKey<T>& key,
                    std::type_identity_t<T> value) && -> Element {
    modifiers_.edit().layout_values.set(key, std::move(value));
    return std::move(*this);
  }

  template <class T>
    requires std::equality_comparable<T> && std::copy_constructible<T>
  auto container_value(const ContainerValueKey<T>& key,
                       std::type_identity_t<T> value) && -> Element {
    membership_.edit().container_values.set(key, std::move(value));
    return std::move(*this);
  }

  auto container_values() const noexcept -> const KeyedValues& {
    return membership_->container_values;
  }

  template <class Compute>
    requires std::convertible_to<
        std::invoke_result_t<const Compute&, const ViewDimensions&>, float>
  auto alignment_guide(AlignmentKey key, Compute compute) && -> Element {
    std::erase_if(
        modifiers_.edit().alignment_guides,
        [&key](const AlignmentGuide& guide) { return guide.key() == key; });
    modifiers_.edit().alignment_guides.emplace_back(key, std::move(compute));
    return std::move(*this);
  }

  auto container_relative_frame(ContainerRelativeFrame frame) && -> Element;

  auto container_relative_frame(
      Axes axes, Alignment alignment = Alignment::center) && -> Element;

  auto container_relative_frame(
      Axes axes, std::uint32_t count, std::uint32_t span, theme::Space spacing,
      Alignment alignment = Alignment::center) && -> Element;

  auto container_relative_frame(
      Axes axes, Alignment alignment,
      ContainerRelativeFrame::Length length) && -> Element;

  auto coordinate_space(ElementKey name) && -> Element;

  auto background(graphics::ShapeStyle style,
                  theme::Shape shape = theme::Rectangle()) && -> Element;

  auto background() && -> Element;

  auto background(graphics::ShapeStyle style, graphics::AnyShape shape,
                  graphics::FillStyle fill_style) && -> Element;

  auto background(graphics::AnyShape shape,
                  graphics::FillStyle fill_style = {}) && -> Element;

  auto background(graphics::ShapeStyle style,
                  theme::Edge ignores_safe_area_edges) && -> Element;

  auto background(theme::Edge ignores_safe_area_edges) && -> Element;

  auto background_style(graphics::ShapeStyle style) && -> Element;

  auto background_prominence(BackgroundProminence prominence) && -> Element;

  auto background(theme::Material material,
                  theme::Shape shape = theme::Rectangle()) && -> Element;

  auto background(Element content) && -> Element;

  auto background(Alignment alignment, Element content) && -> Element;

  auto overlay(Element content) && -> Element;

  auto overlay(Alignment alignment, Element content) && -> Element;

  auto overlay(graphics::ShapeStyle style, theme::Shape shape) && -> Element;

  auto overlay(graphics::ShapeStyle style, graphics::AnyShape shape,
               graphics::FillStyle fill_style = {}) && -> Element;

  auto overlay(graphics::ShapeStyle style,
               theme::Edge ignores_safe_area_edges) && -> Element;

  auto overlay(bool is_presented, OverlayLayer layer,
               Element content) && -> Element;

  auto allows_hit_testing(bool enabled) && -> Element;

  auto accessibility_label(AccessibilityText label) && -> Element;

  auto accessibility_label(AccessibilityText label,
                           bool is_enabled) && -> Element;

  auto accessibility_label(
      std::function<Element(Element label)> content) && -> Element;

  auto accessibility_value(AccessibilityText value) && -> Element;

  auto accessibility_value(AccessibilityText value,
                           bool is_enabled) && -> Element;

  auto accessibility_hint(AccessibilityText hint) && -> Element;

  auto accessibility_hint(AccessibilityText hint,
                          bool is_enabled) && -> Element;

  auto accessibility_identifier(std::string identifier) && -> Element;

  auto accessibility_identifier(std::string identifier,
                                bool is_enabled) && -> Element;

  auto accessibility_add_traits(AccessibilityTraits traits) && -> Element;

  auto accessibility_remove_traits(AccessibilityTraits traits) && -> Element;

  auto accessibility_heading(AccessibilityHeadingLevel level) && -> Element;

  auto accessibility_element(
      AccessibilityChildBehavior children =
          AccessibilityChildBehavior::ignore) && -> Element;

  auto accessibility_hidden(bool hidden = true) && -> Element;

  auto accessibility_hidden(bool hidden, bool is_enabled) && -> Element;

  auto accessibility_input_labels(
      std::vector<AccessibilityText> input_labels) && -> Element;

  auto accessibility_input_labels(std::vector<AccessibilityText> input_labels,
                                  bool is_enabled) && -> Element;

  auto accessibility_sort_priority(double sort_priority) && -> Element;

  auto accessibility_text_content_type(
      AccessibilityTextContentType type) && -> Element;

  auto accessibility_action(std::function<void()> handler) && -> Element;

  auto accessibility_action(AccessibilityActionKind kind,
                            std::function<void()> handler) && -> Element;

  auto accessibility_action(AccessibilityText name,
                            std::function<void()> handler) && -> Element;

  auto accessibility_action(std::function<void()> action,
                            std::function<Element()> label) && -> Element;

  auto accessibility_actions(std::function<Element()> content) && -> Element;

  auto accessibility_actions(AccessibilityActionCategory category,
                             std::function<Element()> content) && -> Element;

  auto accessibility_activation_point(
      motion::UnitPoint activation_point) && -> Element;

  auto accessibility_activation_point(motion::UnitPoint activation_point,
                                      bool is_enabled) && -> Element;

  auto accessibility_activation_point(Point activation_point) && -> Element;

  auto accessibility_activation_point(Point activation_point,
                                      bool is_enabled) && -> Element;

  auto accessibility_adjustable_action(
      std::function<void(AccessibilityAdjustmentDirection)>
          handler) && -> Element;

  auto accessibility_scroll_action(
      std::function<void(theme::Edge)> handler) && -> Element;

  auto accessibility_zoom_action(
      std::function<void(AccessibilityZoomGestureAction)>
          handler) && -> Element;

  auto accessibility_chart_descriptor(
      AccessibilityChartDescriptor descriptor) && -> Element;

  template <AccessibilityChartDescriptorRepresentable R>
  auto accessibility_chart_descriptor(const R& representable) && -> Element {
    return std::move(*this).accessibility_chart_descriptor(
        AccessibilityChartDescriptor{representable.make_chart_descriptor()});
  }

  auto accessibility_children(std::function<Element()> children) && -> Element;

  auto accessibility_representation(
      std::function<Element()> representation) && -> Element;

  auto accessibility_focused(
      AccessibilityFocusBinding<bool> condition) && -> Element;

  template <std::equality_comparable T>
    requires(!std::same_as<T, bool>)
  auto accessibility_focused(AccessibilityFocusBinding<T> binding,
                             std::type_identity_t<T> equals) && -> Element {
    if (binding.active()) {
      interactions_.edit().focused.push_back(
          FocusRequest::of(binding.focus(), std::move(equals)));
    }
    return std::move(*this);
  }

  auto accessibility_default_focus(AccessibilityFocusBinding<bool> binding,
                                   bool value) && -> Element;

  template <std::equality_comparable T>
    requires(!std::same_as<T, bool>)
  auto accessibility_default_focus(
      AccessibilityFocusBinding<T> binding,
      std::type_identity_t<T> value) && -> Element {
    if (!binding.active()) {
      return std::move(*this);
    }
    return std::move(*this).default_focus(binding.focus(), std::move(value));
  }

  auto accessibility_direct_touch(
      bool is_direct_touch_area = true,
      AccessibilityDirectTouchOptions options = {}) && -> Element;

  auto accessibility_drag_point(motion::UnitPoint point,
                                AccessibilityText description) && -> Element;

  auto accessibility_drag_point(motion::UnitPoint point,
                                AccessibilityText description,
                                bool is_enabled) && -> Element;

  auto accessibility_drop_point(motion::UnitPoint point,
                                AccessibilityText description) && -> Element;

  auto accessibility_drop_point(motion::UnitPoint point,
                                AccessibilityText description,
                                bool is_enabled) && -> Element;

  auto accessibility_ignores_invert_colors(bool active = true) && -> Element;

  template <Identifiable ID>
  auto accessibility_labeled_pair(AccessibilityLabeledPairRole role,
                                  const ID& id,
                                  Namespace::ID in) && -> Element {
    return std::move(*this).labeled_pair(
        role,
        AccessibilityAttachment::Pairing{ElementKey::identifying(id), in});
  }

  template <Identifiable ID>
  auto accessibility_linked_group(const ID& id,
                                  Namespace::ID in) && -> Element {
    return std::move(*this).linked_group(
        AccessibilityAttachment::Pairing{ElementKey::identifying(id), in});
  }

  auto accessibility_responds_to_user_interaction(
      bool responds_to_user_interaction = true) && -> Element;

  auto accessibility_responds_to_user_interaction(
      bool responds_to_user_interaction, bool is_enabled) && -> Element;

  auto accessibility_rotor(AccessibilityRotor::Label label,
                           AccessibilityRotor::Entries entries) && -> Element;

  auto accessibility_rotor(AccessibilityRotor::Label label,
                           std::vector<AccessibilityRotorEntry::TextRange>
                               text_ranges) && -> Element;

  template <std::ranges::forward_range Entries, class EntryID, class EntryLabel>
    requires std::copy_constructible<std::ranges::range_value_t<Entries>>
  auto accessibility_rotor(AccessibilityRotor::Label label,
                           const Entries& entries, EntryID entry_id,
                           EntryLabel entry_label) && -> Element {
    std::vector<AccessibilityRotorEntry> made;
    for (const auto& entry : entries) {
      made.emplace_back(
          AccessibilityText{std::string{std::invoke(entry_label, entry)}},
          std::invoke(entry_id, entry));
    }
    return std::move(*this).accessibility_rotor(
        std::move(label), AccessibilityRotor::Entries{[made] { return made; }});
  }

  template <std::ranges::forward_range Entries, class EntryLabel>
    requires requires(const std::ranges::range_value_t<Entries>& entry) {
      entry.id;
    }
  auto accessibility_rotor(AccessibilityRotor::Label label,
                           const Entries& entries,
                           EntryLabel entry_label) && -> Element {
    return std::move(*this).accessibility_rotor(
        std::move(label), entries,
        [](const std::ranges::range_value_t<Entries>& entry) {
          return entry.id;
        },
        std::move(entry_label));
  }

  template <Identifiable ID>
  auto accessibility_rotor_entry(const ID& id, Namespace::ID in) && -> Element {
    return std::move(*this).rotor_entry(
        AccessibilityAttachment::Pairing{ElementKey::identifying(id), in});
  }

  auto accessibility_scroll_status(AccessibilityText status,
                                   bool is_enabled = true) && -> Element;

  auto accessibility_shows_large_content_viewer() && -> Element;

  auto accessibility_shows_large_content_viewer(
      std::function<Element()> large_content_view) && -> Element;

  auto accessibility_custom_content(AccessibilityCustomContentKey key,
                                    AccessibilityText value) && -> Element;

  auto speech_adjusted_pitch(double value) && -> Element;

  auto speech_always_includes_punctuation(bool value = true) && -> Element;

  auto speech_announcements_queued(bool value = true) && -> Element;

  auto speech_spells_out_characters(bool value = true) && -> Element;

  auto assistive_access_navigation_icon(Element icon) && -> Element;

  template <std::convertible_to<std::string_view> Name>
  auto assistive_access_navigation_icon(Name system_image) && -> Element {
    return std::move(*this).navigation_icon(
        AccessibilityAttachment::NavigationIcon{
            .system_image = std::string{std::string_view{system_image}},
            .resource = false});
  }

  auto flips_for_right_to_left_layout_direction(bool enabled) && -> Element;

  auto layout_direction_behavior(
      LayoutDirectionBehavior behavior) && -> Element;

  auto hidden() && -> Element;

  auto exclusive_frame(bool is_presented,
                       std::vector<OverlayLayer> keep = {}) && -> Element;

  auto exclusive_frame(StateHandle<bool> is_presented,
                       std::vector<OverlayLayer> keep = {}) && -> Element;

  auto container_background(
      theme::Fill style, ContainerBackgroundPlacement placement) && -> Element;

  auto container_background(
      theme::Material material,
      ContainerBackgroundPlacement placement) && -> Element;

  auto container_background(
      theme::Glass glass, ContainerBackgroundPlacement placement) && -> Element;

  template <class Content>
    requires std::invocable<const Content&> &&
             std::convertible_to<std::invoke_result_t<const Content&>, Element>
  auto container_background(ContainerBackgroundPlacement placement,
                            Content content) && -> Element {
    return std::move(*this).container_background(placement, Alignment::center,
                                                 std::move(content));
  }

  template <class Content>
    requires std::invocable<const Content&> &&
             std::convertible_to<std::invoke_result_t<const Content&>, Element>
  auto container_background(ContainerBackgroundPlacement placement,
                            Alignment alignment,
                            Content content) && -> Element {
    return std::move(*this).container_background(
        PresentationBackground{
            std::make_shared<const PresentationBackground::View>(
                alignment,
                [content = std::move(content)] {
                  return Element{std::invoke(content)};
                })},
        placement);
  }

  auto clip_shape(theme::Shape shape) && -> Element;

  auto clip_shape(graphics::AnyShape shape,
                  graphics::FillStyle style = {}) && -> Element;

  template <graphics::PathShape S>
  auto clip_shape(S shape, graphics::FillStyle style = {}) && -> Element {
    return std::move(*this).clip_shape(graphics::AnyShape{std::move(shape)},
                                       style);
  }

  auto clipped(bool antialiased = false) && -> Element;

  auto container_shape(theme::Shape shape) && -> Element;

  auto container_corner_offset(theme::Edge edges,
                               bool size_to_fit = false) && -> Element;

  auto border(theme::ColourValue colour,
              theme::Stroke width = 1.0F) && -> Element;

  auto border(graphics::ShapeStyle style, float width) && -> Element;

  auto shadow(theme::ColourValue colour, float radius, float x = 0.0F,
              float y = 0.0F) && -> Element;

  auto shadow(theme::Elevation elevation) && -> Element;

  auto foreground_style(graphics::ShapeStyle style) && -> Element;

  auto foreground_style(graphics::ShapeStyle primary,
                        graphics::ShapeStyle secondary) && -> Element;

  auto foreground_style(graphics::ShapeStyle primary,
                        graphics::ShapeStyle secondary,
                        graphics::ShapeStyle tertiary) && -> Element;

  auto opacity(float value) && -> Element;

  auto brightness(double amount) && -> Element;

  auto contrast(double amount) && -> Element;

  auto saturation(double amount) && -> Element;

  auto grayscale(double amount) && -> Element;

  auto hue_rotation(motion::Angle angle) && -> Element;

  auto color_invert(bool inverted = true) && -> Element;

  auto color_multiply(theme::ColourValue colour) && -> Element;

  auto luminance_to_alpha() && -> Element;

  auto colour_matrix(graphics::ColourMatrix matrix) && -> Element;

  auto colour_matrix(graphics::ColourMatrix matrix, bool premultiplied,
                     bool linear) && -> Element;

  auto blur(float radius, bool opaque = false) && -> Element;

  auto blend_mode(graphics::BlendMode mode) && -> Element;

  auto compositing_group() && -> Element;

  auto drawing_group(bool opaque = false) && -> Element;

  auto rotation_3d_effect(motion::Angle angle, graphics::Rotation3D::Axis axis,
                          motion::UnitPoint anchor = motion::UnitPoint::center,
                          float anchor_z = 0.0F,
                          float perspective = 1.0F) && -> Element;

  auto perspective_rotation_effect(
      motion::Angle angle, graphics::Rotation3D::Axis axis,
      motion::UnitPoint anchor = motion::UnitPoint::center,
      float anchor_z = 0.0F, float perspective = 1.0F) && -> Element;

  auto transform_effect(graphics::AffineTransform transform) && -> Element;

  auto projection_effect(graphics::ProjectionTransform transform) && -> Element;

  auto color_effect(graphics::Shader shader,
                    bool is_enabled = true) && -> Element;

  auto distortion_effect(graphics::Shader shader, Size max_sample_offset,
                         bool is_enabled = true) && -> Element;

  auto layer_effect(graphics::Shader shader, Size max_sample_offset,
                    bool is_enabled = true) && -> Element;

  auto mask(Element content) && -> Element;

  auto mask(Alignment alignment, Element content) && -> Element;

  auto reverse_mask(Alignment alignment, Element content) && -> Element;

  auto glass_effect(theme::Glass glass = theme::Glass::regular(),
                    theme::Shape shape = theme::Shape::capsule()) && -> Element;

  auto glass_panel(components::PanelRadius radius) && -> Element;

  auto glass_effect_id(ElementKey id, Namespace::ID space) && -> Element;

  auto glass_effect_union(ElementKey id, Namespace::ID space) && -> Element;

  auto glass_effect_transition(GlassEffectTransition transition) && -> Element;

  auto default_glass(theme::GlassSlot slot, theme::Shape shape) && -> Element;

  auto scale_effect(float scale, motion::UnitPoint anchor =
                                     motion::UnitPoint::center) && -> Element;

  auto scale_effect(
      float x, float y,
      motion::UnitPoint anchor = motion::UnitPoint::center) && -> Element;

  auto offset(float x, float y = 0.0F) && -> Element;

  auto rotation_effect(
      motion::Angle angle,
      motion::UnitPoint anchor = motion::UnitPoint::center) && -> Element;

  auto transition(motion::Transition transition) && -> Element;

  auto matched_geometry_effect(
      ElementKey id, Namespace::ID space,
      motion::MatchedGeometryProperties properties =
          motion::MatchedGeometryProperties::frame,
      motion::UnitPoint anchor = motion::UnitPoint::center,
      bool is_source = true) && -> Element;

  auto content_transition(motion::ContentTransition transition) && -> Element;

  auto hover_effect(CustomHoverEffect effect = motion::HoverEffect::automatic,
                    bool is_enabled = true) && -> Element;

  auto hover_effect(CustomHoverEffect effect,
                    std::optional<HoverEffectGroup> in,
                    bool is_enabled = true) && -> Element;

  auto hover_effect(CustomHoverEffect::Body body) && -> Element;

  auto hover_effect(std::optional<HoverEffectGroup> in, bool is_enabled,
                    CustomHoverEffect::Body body) && -> Element;

  auto default_hover_effect(
      std::optional<CustomHoverEffect> effect) && -> Element;

  auto hover_effect_disabled(bool disabled = true) && -> Element;

  auto hover_effect_group() && -> Element;

  auto hover_effect_group(std::optional<HoverEffectGroup> group) && -> Element;

  auto hover_effect_group(
      std::optional<std::string> id, Namespace::ID space,
      HoverEffectGroup::Behavior behavior =
          HoverEffectGroup::Behavior::activates_group) && -> Element;

  auto on_hover(std::move_only_function<void(bool)> action) && -> Element;

  auto on_continuous_hover(
      std::move_only_function<void(HoverPhase)> action) && -> Element;

  auto on_continuous_hover(
      CoordinateSpace coordinate_space,
      std::move_only_function<void(HoverPhase)> action) && -> Element;

  auto pointer_style(std::optional<PointerStyle> style) && -> Element;

  auto pointer_visibility(Visibility visibility) && -> Element;

  auto motion_path(motion::MotionPath path, double progress) && -> Element;

  template <class Value, std::equality_comparable Trigger, class Content,
            class Builder>
  auto keyframe_animator(Value initial_value, Trigger trigger, Content content,
                         Builder keyframes) && -> Element;

  template <std::equality_comparable T>
  auto animation(std::optional<motion::Animation> animation,
                 T value) && -> Element {
    motion_.edit().animations.push_back(
        ImplicitAnimation::of(std::move(animation), std::move(value)));
    return std::move(*this);
  }

  auto transaction(TransactionEdit edit) && -> Element;

  template <std::equality_comparable T, class Edit>
    requires std::invocable<Edit&, motion::Transaction&>
  auto transaction(T value, Edit edit) && -> Element {
    motion_.edit().animations.push_back(
        ImplicitAnimation::edited(std::move(value), std::move(edit)));
    return std::move(*this);
  }

  template <class Body>
    requires std::convertible_to<std::invoke_result_t<Body, Element>, Element>
  auto animation(std::optional<motion::Animation> animation,
                 Body body) && -> Element {
    Element scoped{std::invoke(std::move(body), std::move(*this))};
    scoped.motion_.edit().scoped_animation = std::move(animation);
    return scoped;
  }

  template <std::equality_comparable Phase, class Content, class Animate>
    requires std::invocable<Content&, Element, const Phase&>
  auto phase_animator(std::vector<Phase> phases, Content content,
                      Animate animation) && -> Element;

  template <std::equality_comparable Phase, class Content>
    requires std::invocable<Content&, Element, const Phase&>
  auto phase_animator(std::vector<Phase> phases, Content content) && -> Element;

  template <std::equality_comparable Phase, std::equality_comparable Trigger,
            class Content, class Animate>
    requires std::invocable<Content&, Element, const Phase&>
  auto phase_animator(std::vector<Phase> phases, Trigger trigger,
                      Content content, Animate animation) && -> Element;

  template <std::equality_comparable Phase, std::equality_comparable Trigger,
            class Content>
    requires std::invocable<Content&, Element, const Phase&>
  auto phase_animator(std::vector<Phase> phases, Trigger trigger,
                      Content content) && -> Element;

  template <class Effect>
  auto geometry_effect(Effect effect) && -> Element;

  auto geometry_group() && -> Element;

  auto symbol_effect(SymbolEffect effect, bool is_active = true) && -> Element;

  template <std::equality_comparable T>
    requires(!std::same_as<T, bool>)
  auto symbol_effect(SymbolEffect effect, T value) && -> Element;

  template <class T, class Action>
  auto on_change(T value, Action action) && -> Element {
    return std::move(*this).on_change(std::move(value), false,
                                      std::move(action));
  }

  template <class T, class Action>
  auto on_change(T value, bool initial, Action action) && -> Element {
    lifecycle_.edit().changes.push_back(
        ChangeObserver::of(std::move(value), initial, std::move(action)));
    return std::move(*this);
  }

  auto focusable(bool is_focusable = true) && -> Element;

  auto focusable(bool is_focusable,
                 FocusInteractions interactions) && -> Element;

  template <class T>
  auto focused_value(
      const FocusedValueKey<T>& key,
      std::optional<std::type_identity_t<T>> value) && -> Element {
    if (value.has_value()) {
      interactions_.edit().focused_values.push_back(
          key.declare(std::move(*value)));
    }
    return std::move(*this);
  }

  template <class T>
  auto focused_scene_value(
      const FocusedValueKey<T>& key,
      std::optional<std::type_identity_t<T>> value) && -> Element {
    if (value.has_value()) {
      interactions_.edit().scene_values.push_back(
          key.declare(std::move(*value)));
    }
    return std::move(*this);
  }

  template <class T>
  auto focused_object(Observable<T>& object) && -> Element {
    interactions_.edit().focused_values.push_back(
        focused_object_key<T>.declare(FocusedObject<T>{object}));
    return std::move(*this);
  }

  template <class T>
  auto focused_scene_object(Observable<T>& object) && -> Element {
    interactions_.edit().scene_values.push_back(
        focused_object_key<T>.declare(FocusedObject<T>{object}));
    return std::move(*this);
  }

  auto on_key_press(
      KeyEquivalent key,
      std::move_only_function<KeyPress::Result()> action) && -> Element;

  auto on_key_press(KeyEquivalent key, KeyPress::Phases phases,
                    KeyPressHandler::Action action) && -> Element;

  auto on_key_press(std::vector<KeyEquivalent> keys, KeyPress::Phases phases,
                    KeyPressHandler::Action action) && -> Element;

  auto on_key_press(KeyPress::Phases phases,
                    KeyPressHandler::Action action) && -> Element;

  auto on_key_press(std::string characters, KeyPress::Phases phases,
                    KeyPressHandler::Action action) && -> Element;

  auto keyboard_shortcut(
      KeyEquivalent key,
      EventModifiers modifiers = EventModifiers::command) && -> Element;

  auto keyboard_shortcut(
      std::optional<KeyboardShortcut> shortcut) && -> Element;

  auto keyboard_shortcut(
      KeyEquivalent key, EventModifiers modifiers,
      KeyboardShortcut::Localization localization) && -> Element;

  auto on_modifier_keys_changed(
      EventModifiers mask, bool initial,
      ModifierKeysHandler::Action action) && -> Element;

  auto on_modifier_keys_changed(
      ModifierKeysHandler::Action action) && -> Element;

  auto modifier_key_alternate(EventModifiers modifiers,
                              Element alternate) && -> Element;

  auto type_select_equivalent(std::optional<std::string> text) && -> Element;

  auto on_delete_command(Command::Action action) && -> Element;

  auto page_command(Binding<int> value, int lower, int upper,
                    int step = 1) && -> Element;

  template <Transferable T>
  auto copyable(std::vector<T> items) && -> Element;

  template <Transferable T, class Action>
  auto cuttable(std::type_identity<T> type, Action action) && -> Element;

  template <Transferable T, class Action>
  auto paste_destination(std::type_identity<T> type,
                         Action action) && -> Element;

  template <Transferable T, class Action, class Validator>
  auto paste_destination(std::type_identity<T> type, Action action,
                         Validator validator) && -> Element;

  template <class Action>
  auto on_copy_command(Action action) && -> Element;

  template <class Action>
  auto on_cut_command(Action action) && -> Element;

  template <class Action>
  auto on_paste_command(std::vector<UTType> types, Action action) && -> Element;

  template <class Validator, class Action>
    requires std::invocable<Validator&, std::vector<TransferItem>>
  auto on_paste_command(std::vector<UTType> types, Validator validator,
                        Action action) && -> Element;

  template <class Action>
  auto item_provider(Action action) && -> Element;

  template <Transferable T>
  auto draggable(T payload) && -> Element;

  template <Transferable T, class Preview>
  auto draggable(T payload, Preview preview) && -> Element;

  template <Transferable T, class Action>
  auto drop_destination(std::type_identity<T> type, Action action,
                        DropTarget::Targeted is_targeted = {}) && -> Element;

  template <Transferable T, class Action>
    requires std::invocable<Action&, std::vector<T>, const DropSession&>
  auto drop_destination(std::type_identity<T> type, bool is_enabled,
                        Action action) && -> Element;

  auto on_drop(std::vector<UTType> types,
               std::shared_ptr<DropDelegate> delegate) && -> Element;

  auto on_drop(std::vector<UTType> types,
               std::optional<Binding<bool>> is_targeted,
               std::move_only_function<bool(std::vector<TransferItem>)>
                   perform) && -> Element;

  auto on_drop(std::vector<UTType> types,
               std::optional<Binding<bool>> is_targeted,
               std::move_only_function<bool(std::vector<TransferItem>, Point)>
                   perform) && -> Element;

  auto on_drag(const std::function<TransferItem()>& data) && -> Element;

  auto drag_configuration(DragConfiguration configuration) && -> Element;

  auto drop_configuration(
      std::move_only_function<DropConfiguration(const DropSession&)>
          configuration) && -> Element;

  auto on_drag_session_updated(
      std::move_only_function<void(const DragSession&)> action) && -> Element;

  auto on_drop_session_updated(
      std::move_only_function<void(const DropSession&)> action) && -> Element;

  auto drag_previews_formation(
      DragDropPreviewsFormation formation) && -> Element;

  auto drop_previews_formation(
      DragDropPreviewsFormation formation) && -> Element;

  auto spring_loading_behavior(SpringLoadingBehavior behavior) && -> Element;

  auto draggable_container_item(
      ElementKey id, std::optional<Namespace::ID> container_namespace =
                         std::nullopt) && -> Element;

  template <Transferable Item>
    requires requires(const Item& item) { ElementKey{item.id}; }
  auto draggable(std::type_identity<Item> type, std::optional<Item> item,
                 std::optional<Namespace::ID> container_namespace =
                     std::nullopt) && -> Element;

  template <Transferable Item, class Payload>
    requires requires(const Item& item) { ElementKey{item.id}; } &&
             std::convertible_to<std::invoke_result_t<Payload&>,
                                 std::optional<Item>>
  auto draggable(std::type_identity<Item> type,
                 std::optional<Namespace::ID> container_namespace,
                 Payload item) && -> Element;

  template <Transferable Item, class ItemID>
  auto draggable(std::type_identity<Item> type, ItemID Item::* id,
                 std::optional<Item> item,
                 std::optional<Namespace::ID> container_namespace =
                     std::nullopt) && -> Element;

  template <Transferable Item, class ItemID, class Payload>
    requires std::convertible_to<std::invoke_result_t<Payload&>,
                                 std::optional<Item>>
  auto draggable(std::type_identity<Item> type, ItemID Item::* id,
                 std::optional<Namespace::ID> container_namespace,
                 Payload payload) && -> Element;

  template <Transferable Item, class Payload>
    requires requires(const Item& item) { ElementKey{item.id}; }
  auto drag_container(std::type_identity<Item> type,
                      std::optional<Namespace::ID> container_namespace,
                      Payload payload) && -> Element;

  template <Transferable Item, class ItemID, class Payload>
  auto drag_container(std::type_identity<Item> type, ItemID Item::* id,
                      std::optional<Namespace::ID> container_namespace,
                      Payload payload) && -> Element;

  template <class ItemID>
  auto drag_container_selection(
      std::vector<ItemID> selection,
      std::optional<Namespace::ID> container_namespace =
          std::nullopt) && -> Element;

  template <class Item, class Move>
  auto reorder_container(std::type_identity<Item> type, bool is_enabled,
                         Move move) && -> Element;

  template <class Item, class CollectionID, class Move>
  auto reorder_container(std::type_identity<Item> type,
                         std::type_identity<CollectionID> collection,
                         bool is_enabled, Move move) && -> Element;

  template <class Item, class ItemID, class Move>
  auto reorder_container(std::type_identity<Item> type, ItemID Item::* id,
                         bool is_enabled, Move move) && -> Element;

  template <class Item, class ItemID, class CollectionID, class Move>
  auto reorder_container(std::type_identity<Item> type, ItemID Item::* id,
                         std::type_identity<CollectionID> collection,
                         bool is_enabled, Move move) && -> Element;

  auto reorder_item(ElementKey id, std::optional<ElementKey> collection =
                                       std::nullopt) && -> Element;

  auto focus_fallback() && -> Element;

  auto focus_ring_inset(float points) && -> Element;

  auto focused(FocusBinding<bool> condition) && -> Element;

  auto focused(std::unique_ptr<FocusRequest> request) && -> Element;

  template <std::equality_comparable T>
    requires(!std::same_as<T, bool>)
  auto focused(FocusBinding<T> binding,
               std::type_identity_t<T> equals) && -> Element {
    interactions_.edit().focused.push_back(
        FocusRequest::of(std::move(binding), std::move(equals)));
    return std::move(*this);
  }

  auto default_focus(FocusBinding<bool> binding,
                     bool value = true) && -> Element;

  auto default_focus(FocusBinding<bool> binding, bool value,
                     DefaultFocusEvaluationPriority priority) && -> Element;

  template <std::equality_comparable T>
    requires(!std::same_as<T, bool>)
  auto default_focus(
      FocusBinding<T> binding, std::type_identity_t<T> value,
      DefaultFocusEvaluationPriority priority =
          DefaultFocusEvaluationPriority::automatic) && -> Element {
    interactions_.edit().default_focus.push_back(Interactions::DefaultFocus{
        .request = FocusRequest::of(std::move(binding), std::move(value)),
        .priority = priority});
    return std::move(*this);
  }

  auto prefers_default_focus(Namespace::ID in) && -> Element;

  auto prefers_default_focus(bool prefers, Namespace::ID in) && -> Element;

  auto focus_scope(
      Namespace::ID id,
      FocusScopeBehavior behavior = FocusScopeBehavior::open) && -> Element;

  auto focus_section() && -> Element;

  auto disabled(bool is_disabled = true) && -> Element;

  auto focus_effect_disabled(bool disabled = true) && -> Element;

  auto ignores_safe_area(theme::Edge edges = theme::Edge::all) && -> Element;

  auto ignores_safe_area(SafeAreaRegions regions,
                         theme::Edge edges = theme::Edge::all) && -> Element;

  auto ignores_safe_area(SafeAreaRegions regions, theme::Edge edges,
                         std::optional<Alignment> alignment) && -> Element;

  auto safe_area_bar(theme::Edge edge) && -> Element;

  template <class Content>
  auto safe_area_bar(VerticalEdge edge, Content content) && -> Element;

  template <class Content>
  auto safe_area_bar(VerticalEdge edge, HorizontalAlignment alignment,
                     std::optional<float> spacing,
                     Content content) && -> Element;

  template <class Content>
  auto safe_area_bar(HorizontalEdge edge, Content content) && -> Element;

  template <class Content>
  auto safe_area_bar(HorizontalEdge edge, VerticalAlignment alignment,
                     std::optional<float> spacing,
                     Content content) && -> Element;

  template <class Content>
  auto safe_area_inset(VerticalEdge edge, Content content) && -> Element;

  template <class Content>
  auto safe_area_inset(VerticalEdge edge, HorizontalAlignment alignment,
                       std::optional<float> spacing,
                       Content content) && -> Element;

  template <class Content>
  auto safe_area_inset(HorizontalEdge edge, Content content) && -> Element;

  template <class Content>
  auto safe_area_inset(HorizontalEdge edge, VerticalAlignment alignment,
                       std::optional<float> spacing,
                       Content content) && -> Element;

  auto scaffold_bar_placement(theme::Edge edge,
                              BarPlacement placement) && -> Element;

  auto font(theme::Font value) && -> Element;

  auto multiline_text_alignment(TextAlignment alignment) && -> Element;

  auto multiline_text_alignment(TextAlignmentStrategy strategy) && -> Element;

  auto writing_direction(WritingDirectionStrategy strategy) && -> Element;

  auto line_limit(std::optional<std::size_t> lines) && -> Element;

  auto line_limit(std::size_t lines, bool reserves_space) && -> Element;

  auto line_limit(LinesBetween lines) && -> Element;

  auto line_limit(LinesAtLeast lines) && -> Element;

  auto line_height(std::optional<text::LineHeight> height) && -> Element;

  auto typesetting_language(text::TypesettingLanguage language,
                            bool is_enabled = true) && -> Element;

  auto text_variant(TextVariantPreference preference) && -> Element;

  template <class Renderer>
  auto text_renderer(Renderer renderer) && -> Element;

  template <class Selectability>
  auto text_selection(Selectability selectability) && -> Element;

  auto text_selection_affinity(TextSelectionAffinity affinity) && -> Element;

  template <class Shape>
  auto text_input_border_shape(Shape shape) && -> Element;

  template <class Content>
  auto text_input_suggestions(Content content) && -> Element;

  template <class Data, class Build>
  auto text_input_suggestions(Data data, Build build) && -> Element;

  template <class Data, class Id, class Build>
  auto text_input_suggestions(Data data, Id id, Build build) && -> Element;

  template <class Completion>
  auto text_input_completion(Completion completion) && -> Element;

  auto text_input_formatting_control_visibility(
      Visibility visibility,
      TextInputFormattingControlPlacement::Set placements) && -> Element;

  template <class Definition>
  auto attributed_text_formatting_definition(
      Definition definition) && -> Element;

  auto find_navigator(Binding<bool> is_presented) && -> Element;

  auto find_disabled(bool is_disabled = true) && -> Element;

  auto replace_disabled(bool is_disabled = true) && -> Element;

  auto search_selection(
      Binding<std::optional<TextSelection>> selection) && -> Element;

  auto truncation_mode(TruncationMode mode) && -> Element;

  auto minimum_scale_factor(float factor) && -> Element;

  auto line_spacing(float spacing) && -> Element;

  auto allows_tightening(bool flag = true) && -> Element;

  auto text_case(std::optional<TextCase> text_case) && -> Element;

  auto kerning(float kerning) && -> Element;

  auto tracking(float tracking) && -> Element;

  auto baseline_offset(float offset) && -> Element;

  auto monospaced_digit(bool is_active = true) && -> Element;

  auto monospaced(bool is_active = true) && -> Element;

  auto italic(bool is_active = true) && -> Element;

  auto bold(bool is_active = true) && -> Element;

  auto font_weight(std::optional<theme::Font::Weight> weight) && -> Element;

  auto font_design(std::optional<theme::Font::Design> design) && -> Element;

  auto font_width(std::optional<theme::Font::Width> width) && -> Element;

  auto text_scale(TextScale scale, bool is_enabled = true) && -> Element;

  auto underline(
      bool is_active = true,
      std::optional<theme::ColourValue> colour = std::nullopt) && -> Element;

  auto underline(
      bool is_active, text::LinePattern pattern,
      std::optional<theme::ColourValue> colour = std::nullopt) && -> Element;

  auto strikethrough(
      bool is_active = true,
      std::optional<theme::ColourValue> colour = std::nullopt) && -> Element;

  auto strikethrough(
      bool is_active, text::LinePattern pattern,
      std::optional<theme::ColourValue> colour = std::nullopt) && -> Element;

  auto redacted(RedactionReasons reason) && -> Element;

  auto unredacted() && -> Element;

  auto privacy_sensitive(bool sensitive = true) && -> Element;

  auto image_scale(ImageScale scale) && -> Element;

  auto symbol_variant(SymbolVariants variant) && -> Element;

  auto symbol_rendering_mode(SymbolRenderingMode mode) && -> Element;

  auto symbol_color_rendering_mode(
      std::optional<SymbolColorRenderingMode> mode) && -> Element;

  auto symbol_variable_value_mode(
      std::optional<SymbolVariableValueMode> mode) && -> Element;

  auto symbol_effects_removed(bool is_enabled = true) && -> Element;

  auto allowed_dynamic_range(
      std::optional<ImageDynamicRange> range) && -> Element;

  template <class Session>
  auto async_image_url_session(Session session) && -> Element;

  auto aspect_ratio(std::optional<float> ratio = std::nullopt,
                    ContentMode mode = ContentMode::fit) && -> Element;

  auto scaled_to_fit() && -> Element;

  auto scaled_to_fill() && -> Element;

  auto grid_cell_columns(std::uint32_t count) && -> Element;

  auto grid_column_alignment(HorizontalAlignment guide) && -> Element;

  auto grid_cell_anchor(motion::UnitPoint anchor) && -> Element;

  auto grid_cell_unsized_axes(Axes axes) && -> Element;

  auto list_row_insets(std::optional<theme::Insets> insets) && -> Element;

  auto list_row_insets(theme::Edge edges,
                       std::optional<float> length) && -> Element;

  auto list_row_separator(
      Visibility visibility,
      VerticalEdges edges = VerticalEdges::all) && -> Element;

  auto list_row_separator_tint(
      std::optional<theme::ColourValue> colour,
      VerticalEdges edges = VerticalEdges::all) && -> Element;

  auto list_row_background(Element view) && -> Element;

  auto list_item_tint(std::optional<theme::Colour> colour) && -> Element;

  auto list_item_tint(ListItemTint tint) && -> Element;

  auto labels_hidden() && -> Element;

  auto labels_visibility(Visibility visibility) && -> Element;

  auto menu_indicator(Visibility visibility) && -> Element;

  auto label_icon_to_title_spacing(float value) && -> Element;

  auto label_reserved_icon_width(float value) && -> Element;

  auto slider_thumb_visibility(Visibility visibility) && -> Element;

  auto horizontal_radio_group_layout() && -> Element;

  template <class Effect>
  auto palette_selection_effect(Effect effect) && -> Element;

  template <class Destination>
  auto share_destination(Destination destination) && -> Element;

  auto menu_order(MenuOrder order) && -> Element;

  auto menu_action_dismiss_behavior(
      MenuActionDismissBehavior behavior) && -> Element;

  auto badge(int count) && -> Element;

  auto badge(std::optional<std::string> label) && -> Element;

  auto selection_disabled(bool disabled = true) && -> Element;

  auto move_disabled(bool disabled = true) && -> Element;

  auto delete_disabled(bool disabled = true) && -> Element;

  auto swipe_actions(std::function<Element()> content) && -> Element;

  auto swipe_actions(HorizontalEdge edge, bool allows_full_swipe,
                     std::function<Element()> content) && -> Element;

  auto list_row_hover_effect(
      std::optional<motion::HoverEffect> effect) && -> Element;

  auto list_row_hover_effect_disabled(bool disabled = true) && -> Element;

  auto list_row_spacing(std::optional<float> spacing) && -> Element;

  auto list_section_spacing(std::optional<float> spacing) && -> Element;

  auto default_min_list_row_height(std::optional<float> height) && -> Element;

  auto alternating_row_backgrounds(bool enabled = true) && -> Element;

  auto alternating_row_backgrounds(
      AlternatingRowBackgroundBehavior behavior) && -> Element;

  auto header_prominence(Prominence prominence) && -> Element;

  auto list_section_index_visibility(Visibility visibility) && -> Element;

  auto section_index_label(std::optional<std::string> label) && -> Element;

  auto list_section_spacing(ListSectionSpacing spacing) && -> Element;

  auto list_section_margins(theme::Edge edges,
                            std::optional<float> length) && -> Element;

  auto list_section_separator(
      Visibility visibility,
      VerticalEdges edges = VerticalEdges::all) && -> Element;

  auto list_section_separator_tint(
      std::optional<theme::ColourValue> colour,
      VerticalEdges edges = VerticalEdges::all) && -> Element;

  auto default_min_list_header_height(
      std::optional<float> height) && -> Element;

  auto badge_prominence(BadgeProminence prominence) && -> Element;

  template <class Host = SwipeHost>
  auto swipe_actions_container() && -> Element;

  auto section_actions(std::function<Element()> content) && -> Element;

  auto table_column_headers(Visibility visibility) && -> Element;

  template <class Style>
  auto table_style(Style style) && -> Element;

  template <class Selected, class Menu>
  auto context_menu(std::type_identity<Selected> type, Menu menu) && -> Element;

  template <class Selected, class Menu, class Primary>
  auto context_menu(std::type_identity<Selected> type, Menu menu,
                    Primary primary_action) && -> Element;

  auto has_swipe_actions() const noexcept -> bool;

  auto list_row() const noexcept -> std::shared_ptr<const ListRow>;

  template <class Text>
  auto searchable(
      Text text,
      SearchFieldPlacement placement = SearchFieldPlacement::automatic,
      std::optional<std::string> prompt = std::nullopt) && -> Element;

  template <class Text>
  auto searchable(
      Text text, Binding<bool> is_presented,
      SearchFieldPlacement placement = SearchFieldPlacement::automatic,
      std::optional<std::string> prompt = std::nullopt) && -> Element;

  template <class Text, class Tokens, class Token>
    requires BindingSource<Tokens> &&
             requires { typename BoundValue<Tokens>::value_type; }
  auto searchable(
      Text text, Tokens tokens, Token token,
      SearchFieldPlacement placement = SearchFieldPlacement::automatic,
      std::optional<std::string> prompt = std::nullopt) && -> Element;

  template <class Text, class Tokens, class Token>
    requires BindingSource<Tokens> &&
             requires { typename BoundValue<Tokens>::value_type; }
  auto searchable(
      Text text, Tokens tokens, Binding<bool> is_presented, Token token,
      SearchFieldPlacement placement = SearchFieldPlacement::automatic,
      std::optional<std::string> prompt = std::nullopt) && -> Element;

  template <class Text, class Tokens, class Suggested, class Token>
    requires BindingSource<Tokens> && BindingSource<Suggested> &&
             requires { typename BoundValue<Tokens>::value_type; } &&
             requires { typename BoundValue<Suggested>::value_type; }
  auto searchable(
      Text text, Tokens tokens, Suggested suggested_tokens, Token token,
      SearchFieldPlacement placement = SearchFieldPlacement::automatic,
      std::optional<std::string> prompt = std::nullopt) && -> Element;

  template <class Text, class Tokens, class Suggested, class Token>
    requires BindingSource<Tokens> && BindingSource<Suggested> &&
             requires { typename BoundValue<Tokens>::value_type; } &&
             requires { typename BoundValue<Suggested>::value_type; }
  auto searchable(
      Text text, Tokens tokens, Suggested suggested_tokens,
      Binding<bool> is_presented, Token token,
      SearchFieldPlacement placement = SearchFieldPlacement::automatic,
      std::optional<std::string> prompt = std::nullopt) && -> Element;

  template <class Text, class T, class Token>
  auto searchable(
      Text text, EditableTokens<T> editable_tokens, Token token,
      SearchFieldPlacement placement = SearchFieldPlacement::automatic,
      std::optional<std::string> prompt = std::nullopt) && -> Element;

  template <class Text, class T, class Token>
  auto searchable(
      Text text, EditableTokens<T> editable_tokens, Binding<bool> is_presented,
      Token token,
      SearchFieldPlacement placement = SearchFieldPlacement::automatic,
      std::optional<std::string> prompt = std::nullopt) && -> Element;

  auto search_suggestions(
      Visibility visibility,
      SearchSuggestionsPlacement::Set placements) && -> Element;

  auto search_presentation_toolbar_behavior(
      SearchPresentationToolbarBehavior behavior) && -> Element;

  auto search_toolbar_behavior(SearchToolbarBehavior behavior) && -> Element;

  template <class Suggestions>
  auto search_suggestions(Suggestions suggestions) && -> Element;

  template <class Scope, class Scopes>
    requires BindingSource<Scope>
  auto search_scopes(Scope scope, Scopes scopes,
                     SearchScopeActivation activation =
                         SearchScopeActivation::automatic) && -> Element;

  template <class Completion>
  auto search_completion(Completion completion) && -> Element;

  auto search_focused(FocusBinding<bool> focused) && -> Element;

  template <std::equality_comparable T>
    requires(!std::same_as<T, bool>)
  auto search_focused(FocusBinding<T> focused,
                      std::type_identity_t<T> equals) && -> Element;

  template <class Items>
  auto context_menu(Items items) && -> Element;

  template <class Items, class Preview>
  auto context_menu(Items items, Preview preview) && -> Element;

  auto tint(std::optional<graphics::ShapeStyle> style) && -> Element;

  auto preferred_color_scheme(
      std::optional<theme::ColorScheme> scheme) && -> Element;

  auto defers_system_gestures(theme::Edge edges) && -> Element;

  auto on_submit(std::move_only_function<void()> action) && -> Element;

  auto on_submit(SubmitTriggers triggers,
                 std::move_only_function<void()> action) && -> Element;

  auto submit_scope(bool is_blocking = true) && -> Element;

  auto submit_label(provider::SubmitLabel label) && -> Element;

  auto keyboard_type(provider::KeyboardType type) && -> Element;

  auto text_content_type(
      std::optional<provider::TextContentType> type) && -> Element;

  auto text_input_autocapitalization(
      std::optional<provider::TextInputAutocapitalization> style) && -> Element;

  auto autocorrection_disabled(bool disable = true) && -> Element;

  auto on_move_command(Command::MoveAction action) && -> Element;

  auto on_move_command(Command::MoveAction action, Axis axis) && -> Element;

  auto on_focus_edge(MoveCommandDirection direction,
                     Command::Action action) && -> Element;

  auto on_exit_command(Command::Action action) && -> Element;

  auto on_exit_command(std::string label, Command::Action action) && -> Element;

  auto on_exit_command(std::string label, Command::Action action,
                       CommandOptions options) && -> Element;

  auto on_play_pause_command(Command::Action action) && -> Element;

  auto on_play_pause_command(std::string label,
                             Command::Action action) && -> Element;

  auto on_command(provider::Button button, std::string label,
                  Command::Action action,
                  CommandOptions options = {}) && -> Element;

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  auto sensory_feedback(SensoryFeedback feedback, T trigger) && -> Element {
    return std::move(*this).feedback(
        FeedbackOrigin::application,
        FeedbackTrigger::fixed(feedback, std::move(trigger)));
  }

  template <std::equality_comparable T, FeedbackResolver<T> Resolve>
    requires std::copy_constructible<T>
  auto sensory_feedback(T trigger, Resolve resolve) && -> Element {
    return std::move(*this).feedback(
        FeedbackOrigin::application,
        FeedbackTrigger::resolved(std::move(trigger), std::move(resolve)));
  }

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  auto default_sensory_feedback(SensoryFeedback feedback,
                                T trigger) && -> Element {
    return std::move(*this).feedback(
        FeedbackOrigin::system,
        FeedbackTrigger::fixed(feedback, std::move(trigger)));
  }

  template <std::equality_comparable T, FeedbackResolver<T> Resolve>
    requires std::copy_constructible<T>
  auto default_sensory_feedback(T trigger, Resolve resolve) && -> Element {
    return std::move(*this).feedback(
        FeedbackOrigin::system,
        FeedbackTrigger::resolved(std::move(trigger), std::move(resolve)));
  }

  template <std::equality_comparable T, class Condition>
    requires std::copy_constructible<T> && std::copy_constructible<Condition> &&
             std::predicate<const Condition&, const T&, const T&>
  auto sensory_feedback(SensoryFeedback feedback, T trigger,
                        Condition condition) && -> Element {
    return std::move(*this).feedback(
        FeedbackOrigin::application,
        FeedbackTrigger::resolved(std::move(trigger),
                                  [feedback, condition = std::move(condition)](
                                      const T& before, const T& after)
                                      -> std::optional<SensoryFeedback> {
                                    if (std::invoke(condition, before, after)) {
                                      return feedback;
                                    }
                                    return std::nullopt;
                                  }));
  }

  auto sensory_feedback_disabled(bool disabled = true) && -> Element;

  auto on_tap_gesture(std::move_only_function<void()> action) && -> Element;

  auto on_tap_gesture(int count,
                      std::move_only_function<void()> action) && -> Element;

  auto on_long_press_gesture(
      std::move_only_function<void()> perform) && -> Element;

  auto on_long_press_gesture(
      double minimum_duration, std::move_only_function<void()> perform,
      LongPressGesture::Pressing on_pressing_changed = {}) && -> Element;

  auto on_long_press_gesture(
      double minimum_duration, float maximum_distance,
      std::move_only_function<void()> perform,
      LongPressGesture::Pressing on_pressing_changed = {}) && -> Element;

  auto gesture(Gesture gesture) && -> Element;

  auto simultaneous_gesture(Gesture gesture) && -> Element;

  auto high_priority_gesture(Gesture gesture) && -> Element;

  auto gesture(Gesture gesture, GestureMask mask) && -> Element;

  auto gesture(Gesture gesture, bool is_enabled) && -> Element;

  auto simultaneous_gesture(Gesture gesture, GestureMask mask) && -> Element;

  auto simultaneous_gesture(Gesture gesture, bool is_enabled) && -> Element;

  auto high_priority_gesture(Gesture gesture, GestureMask mask) && -> Element;

  auto high_priority_gesture(Gesture gesture, bool is_enabled) && -> Element;

  auto on_tap_gesture(
      int count, CoordinateSpace coordinate_space,
      std::move_only_function<void(Point)> action) && -> Element;

  auto on_tap_gesture(
      int count, CoordinateSpace coordinate_space,
      GestureInputKinds input_kinds,
      std::move_only_function<void(Point)> action) && -> Element;

  auto on_long_press_gesture(
      double minimum_duration, float maximum_distance,
      GestureInputKinds input_kinds, std::move_only_function<void()> perform,
      LongPressGesture::Pressing on_pressing_changed = {}) && -> Element;

  template <class Name>
    requires(std::convertible_to<Name, std::string_view> &&
             !std::same_as<std::remove_cvref_t<Name>, bool>)
  auto gesture(Gesture gesture, Name&& name,
               bool is_enabled = true) && -> Element {
    return std::move(*this).named_gesture(std::move(gesture),
                                          GesturePriority::normal,
                                          std::string_view{name}, is_enabled);
  }

  template <class Name>
    requires(std::convertible_to<Name, std::string_view> &&
             !std::same_as<std::remove_cvref_t<Name>, bool>)
  auto simultaneous_gesture(Gesture gesture, Name&& name,
                            bool is_enabled = true) && -> Element {
    return std::move(*this).named_gesture(std::move(gesture),
                                          GesturePriority::simultaneous,
                                          std::string_view{name}, is_enabled);
  }

  template <class Name>
    requires(std::convertible_to<Name, std::string_view> &&
             !std::same_as<std::remove_cvref_t<Name>, bool>)
  auto high_priority_gesture(Gesture gesture, Name&& name,
                             bool is_enabled = true) && -> Element {
    return std::move(*this).named_gesture(std::move(gesture),
                                          GesturePriority::high,
                                          std::string_view{name}, is_enabled);
  }

  auto content_shape(theme::Shape shape) && -> Element;

  auto content_shape(ContentShapeKinds kinds, theme::Shape shape) && -> Element;

  auto content_shape(graphics::AnyShape shape, bool eo_fill) && -> Element;

  auto content_shape(ContentShapeKinds kinds, graphics::AnyShape shape,
                     bool eo_fill = false) && -> Element;

  auto scroll_disabled(bool disabled = true) && -> Element;

  auto scroll_target_behavior(ScrollTargetBehavior behavior) && -> Element;

  auto scroll_target_behavior(AnyScrollTargetBehavior behavior) && -> Element;

  auto scroll_target_behavior(
      PagingScrollTargetBehavior behavior) && -> Element;

  auto scroll_target_behavior(
      ViewAlignedScrollTargetBehavior behavior) && -> Element;

  auto scroll_target_layout(bool is_enabled = true) && -> Element;

  auto scroll_indicators(ScrollIndicatorVisibility visibility,
                         Axes axes = Axes::all) && -> Element;

  auto scroll_indicators_flash(bool on_appear) && -> Element;

  template <Identifiable T>
    requires(!std::same_as<T, bool>)
  auto scroll_indicators_flash(const T& trigger) && -> Element {
    interactions_.edit().scrolling.flash = Scrolling::Flash{
        .trigger = ElementKey::identifying(trigger), .on_appear = false};
    return std::move(*this);
  }

  auto scroll_bounce_behavior(ScrollBounceBehavior behavior,
                              Axes axes = Axes::vertical) && -> Element;

  auto scroll_deceleration_rate(ScrollDecelerationRate rate) && -> Element;

  auto scroll_dismisses_keyboard(
      ScrollDismissesKeyboardMode mode) && -> Element;

  auto scroll_input_behavior(ScrollInputBehavior behavior,
                             ScrollInputKind input) && -> Element;

  auto default_scroll_anchor(
      std::optional<motion::UnitPoint> anchor) && -> Element;

  auto default_scroll_anchor(std::optional<motion::UnitPoint> anchor,
                             ScrollAnchorRole role) && -> Element;

  auto scroll_position(
      Binding<ScrollPosition> position,
      std::optional<motion::UnitPoint> anchor = std::nullopt) && -> Element;

  auto scroll_content_background(Visibility visibility) && -> Element;

  auto scroll_edge_effect_style(std::optional<ScrollEdgeEffectStyle> style,
                                theme::Edge edges) && -> Element;

  auto scroll_edge_effect_hidden(
      bool hidden = true, theme::Edge edges = theme::Edge::all) && -> Element;

  auto background_extension_effect() && -> Element;

  auto content_margins(theme::Space length,
                       ContentMarginPlacement placement =
                           ContentMarginPlacement::automatic) && -> Element;

  auto content_margins(theme::Insets insets,
                       ContentMarginPlacement placement =
                           ContentMarginPlacement::automatic) && -> Element;

  auto content_margins(theme::Edge edges, theme::Space length,
                       ContentMarginPlacement placement =
                           ContentMarginPlacement::automatic) && -> Element;

  auto content_margins(
      ContentMarginGuide guide, theme::Edge edges = theme::Edge::all,
      std::optional<Alignment> alignment = std::nullopt) && -> Element;

  auto safe_area_padding(theme::Space length) && -> Element;

  auto safe_area_padding(theme::Insets insets) && -> Element;

  auto safe_area_padding(theme::Edge edges, theme::Space length) && -> Element;

  auto scroll_clip_disabled(bool disabled = true) && -> Element;

  auto scroll_transition(ScrollEffects::Transition transition) && -> Element;

  auto scroll_transition(ScrollTransitionConfiguration configuration,
                         ScrollEffects::Transition transition) && -> Element;

  auto scroll_transition(ScrollTransitionConfiguration configuration, Axis axis,
                         ScrollEffects::Transition transition) && -> Element;

  auto scroll_transition(ScrollTransitionConfiguration top_leading,
                         ScrollTransitionConfiguration bottom_trailing,
                         ScrollEffects::Transition transition) && -> Element;

  auto scroll_transition(ScrollTransitionConfiguration top_leading,
                         ScrollTransitionConfiguration bottom_trailing,
                         Axis axis,
                         ScrollEffects::Transition transition) && -> Element;

  auto visual_effect(ScrollEffects::Effect effect) && -> Element;

  auto geometry_projection(ScrollEffects::Projection projection) && -> Element;

  auto on_scroll_phase_change(Scrolling::PhaseAction action) && -> Element;

  auto on_scroll_phase_change(
      Scrolling::PhaseContextAction action) && -> Element;

  auto on_scroll_visibility_change(
      std::move_only_function<void(bool)> action) && -> Element;

  auto on_scroll_visibility_change(
      double threshold,
      std::move_only_function<void(bool)> action) && -> Element;

  template <class T, class Action>
  auto on_scroll_target_visibility_change(std::type_identity<T>,
                                          double threshold,
                                          Action action) && -> Element {
    interactions_.edit().scrolling.target_visibility_observers.push_back(
        std::make_shared<ScrollTargetVisibilityObserver::Typed<T>>(
            threshold, std::move(action)));
    return std::move(*this);
  }

  template <class T>
  auto scroll_position(
      StateHandle<std::optional<T>> id,
      std::optional<motion::UnitPoint> anchor = std::nullopt) && -> Element {
    interactions_.edit().scrolling.position =
        std::make_shared<ScrollPositionBinding::Typed<T>>(
            Binding<std::optional<T>>{std::move(id)}, anchor);
    return std::move(*this);
  }

  template <class T>
  auto scroll_position(
      Binding<std::optional<T>> id,
      std::optional<motion::UnitPoint> anchor = std::nullopt) && -> Element {
    interactions_.edit().scrolling.position =
        std::make_shared<ScrollPositionBinding::Typed<T>>(std::move(id),
                                                          anchor);
    return std::move(*this);
  }

  auto refresh_trigger(std::move_only_function<void()> trigger) && -> Element;

  template <class Action>
  auto refreshable(Action action) && -> Element;

  template <class Transform, class Action>
    requires std::invocable<const Transform&, const ScrollGeometry&>
  auto on_scroll_geometry_change(Transform transform,
                                 Action action) && -> Element {
    using Value = std::remove_cvref_t<
        std::invoke_result_t<const Transform&, const ScrollGeometry&>>;
    interactions_.edit().scrolling.geometry_observers.push_back(
        std::make_shared<ScrollGeometryObserver::Typed<Value>>(
            std::move(transform), std::move(action)));
    return std::move(*this);
  }

  template <class T, class Transform, class Action>
    requires std::convertible_to<
                 std::invoke_result_t<const Transform&, const GeometryProxy&>,
                 T> &&
             std::equality_comparable<T> && std::invocable<Action&, const T&>
  auto on_geometry_change(std::type_identity<T>, Transform of,
                          Action action) && -> Element {
    interactions_.edit().geometry.push_back(
        std::make_shared<GeometryObserver::Typed<T>>(std::move(of),
                                                     std::move(action)));
    return std::move(*this);
  }

  template <class T, class Transform, class Action>
    requires std::convertible_to<
                 std::invoke_result_t<const Transform&, const GeometryProxy&>,
                 T> &&
             std::equality_comparable<T> &&
             std::invocable<Action&, const T&, const T&>
  auto on_geometry_change(std::type_identity<T>, Transform of,
                          Action action) && -> Element {
    interactions_.edit().geometry.push_back(
        std::make_shared<GeometryObserver::Changing<T>>(std::move(of),
                                                        std::move(action)));
    return std::move(*this);
  }

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  auto tag(T value) && -> Element {
    membership_.edit().tag = TagValue::of(std::move(value));
    return std::move(*this);
  }

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  auto tag(T value, bool include_optional) && -> Element {
    membership_.edit().tag = TagValue::of(std::move(value), include_optional);
    return std::move(*this);
  }

  auto tag() const noexcept -> const std::shared_ptr<const TagValue>& {
    return membership_->tag;
  }

  template <class Style>
  auto button_style(Style style) && -> Element;

  template <class Style>
  auto toggle_style(Style style) && -> Element;

  template <class Style>
  auto menu_style(Style style) && -> Element;

  template <class Style>
  auto form_style(Style style) && -> Element;

  template <class Style>
  auto group_box_style(Style style) && -> Element;

  template <class Style>
  auto labeled_content_style(Style style) && -> Element;

  template <class Style>
  auto arrangement_view_style(Style style) && -> Element;

  template <class Style>
  auto control_group_style(Style style) && -> Element;

  template <class Style>
  auto disclosure_group_style(Style style) && -> Element;

  template <class Style>
  auto gauge_style(Style style) && -> Element;

  template <class Style>
  auto text_editor_style(Style style) && -> Element;

  template <class Style>
  auto date_picker_style(Style style) && -> Element;

  template <class Style>
  auto picker_style(Style style) && -> Element;

  template <class Style>
  auto progress_view_style(Style style) && -> Element;

  template <class Style>
  auto label_style(Style style) && -> Element;

  template <class Style>
  auto text_field_style(Style style) && -> Element;

  template <class Style>
  auto list_style(Style style) && -> Element;

  template <class Style>
  auto cell_style(Style style) && -> Element;

  template <class Style>
  auto tab_view_style(Style style) && -> Element;

  template <class First, class... Items>
    requires(!std::convertible_to<First, std::string_view>)
  auto toolbar(First first, Items... items) && -> Element {
    add_toolbar(std::move(first));
    (add_toolbar(std::move(items)), ...);
    return std::move(*this);
  }

  template <class... Items>
  auto toolbar(std::string id, Items... items) && -> Element {
    const auto named = std::make_shared<const std::string>(std::move(id));
    (add_toolbar(named, std::move(items)), ...);
    return std::move(*this);
  }

  auto toolbar(ToolbarRemoval removal) && -> Element;

  auto toolbar_item_hidden(bool hidden = true) && -> Element;

  template <class Content>
  auto toolbar_overflow_menu(Content content) && -> Element;

  auto toolbar_title_alignment(ToolbarTitleAlignment alignment) && -> Element;

  auto toolbar_minimization_behavior(
      ToolbarMinimizationBehavior behavior,
      ToolbarPlacement bars = ToolbarPlacement::automatic) && -> Element;

  auto toolbar_minimization_restoration(
      ToolbarMinimizationRestoration restoration,
      ToolbarPlacement bars = ToolbarPlacement::automatic) && -> Element;

  auto toolbar_minimization_safe_area_adjustment(
      ToolbarMinimizationSafeAreaAdjustment adjustment,
      ToolbarPlacement bars = ToolbarPlacement::automatic) && -> Element;

  auto toolbar_vertical_behavior(
      ToolbarVerticalBehavior behavior) && -> Element;

  auto toolbar_vertical_compression_behavior(
      ToolbarVerticalCompressionBehavior behavior) && -> Element;

  template <class Theme>
  auto theme_override(Theme value) && -> Element;

  template <class Size>
  auto dynamic_type_size(Size size) && -> Element;

  template <class Forms>
  auto sidebar_collapse(Forms forms) && -> Element;

  template <class Delay>
  auto detail_settle_delay(Delay delay) && -> Element;

  auto navigation_title(std::string title) && -> Element;

  auto navigation_transition(NavigationTransition transition) && -> Element;

  auto matched_transition_source(ElementKey id,
                                 Namespace::ID space) && -> Element;

  auto matched_transition_source(
      ElementKey id, Namespace::ID space,
      const std::function<MatchedTransitionSourceConfiguration(
          MatchedTransitionSourceConfiguration)>& configuration) && -> Element;

  auto navigation_subtitle(std::string subtitle) && -> Element;

  auto navigation_link_indicator_visibility(
      Visibility visibility) && -> Element;

  auto navigation_bar_back_button_hidden(bool hides = true) && -> Element;

  auto navigation_bar_title_display_mode(
      NavigationBarTitleDisplayMode mode) && -> Element;

  auto toolbar_title_display_mode(ToolbarTitleDisplayMode mode) && -> Element;

  auto toolbar_role(ToolbarRole role) && -> Element;

  auto toolbar_visibility(
      Visibility visibility,
      ToolbarPlacement bars = ToolbarPlacement::automatic) && -> Element;

  auto toolbar_background(
      theme::ColourValue style,
      ToolbarPlacement bars = ToolbarPlacement::automatic) && -> Element;

  auto toolbar_background(
      Visibility visibility,
      ToolbarPlacement bars = ToolbarPlacement::automatic) && -> Element;

  auto toolbar_color_scheme(
      std::optional<theme::ColorScheme> scheme,
      ToolbarPlacement bars = ToolbarPlacement::automatic) && -> Element;

  auto toolbar_foreground_style(
      theme::ColourValue style,
      ToolbarPlacement bars = ToolbarPlacement::automatic) && -> Element;

  auto toolbar_title_menu(std::function<Element()> content) && -> Element;

  template <class Content>
  auto navigation_destination(Binding<bool> is_presented,
                              Content content) && -> Element;

  template <PresentedItem Item, class Build>
  auto navigation_destination(Item item, Build build) && -> Element;

  auto navigation_split_view_column_width(float width) && -> Element;

  auto navigation_split_view_column_width(
      std::optional<float> minimum, float ideal,
      std::optional<float> maximum) && -> Element;

  auto navigation_split_view_style(
      NavigationSplitViewStyle style) && -> Element;

  template <class Style>
    requires(!std::convertible_to<Style, NavigationSplitViewStyle>)
  auto navigation_split_view_style(Style style) && -> Element;

  template <class Style>
  auto navigation_view_style(Style style) && -> Element;

  template <class Content>
  auto tab_view_sidebar_header(Content content) && -> Element;

  template <class Content>
  auto tab_view_sidebar_footer(Content content) && -> Element;

  template <class Content>
  auto tab_view_sidebar_bottom_bar(Content content) && -> Element;

  template <class Placement, class Content>
  auto content_toolbar(Placement placement, Content content) && -> Element;

  template <class Content>
  auto tab_view_bottom_accessory(Content content) && -> Element;

  template <class Content>
  auto tab_view_bottom_accessory(bool is_enabled,
                                 Content content) && -> Element;

  template <class Behavior>
  auto tab_bar_minimize_behavior(Behavior behavior) && -> Element;

  template <class Activation>
  auto tab_view_search_activation(Activation activation) && -> Element;

  template <class Placement>
  auto default_tab_bar_placement(Placement placement) && -> Element;

  template <class Placement>
  auto default_adaptable_tab_bar_placement(Placement placement) && -> Element;

  template <class T, class Build>
    requires std::convertible_to<std::invoke_result_t<const Build&, const T&>,
                                 Element>
  auto navigation_destination(std::type_identity<T> type,
                              Build build) && -> Element;

  auto interactive_dismiss_disabled(bool disabled = true) && -> Element;

  auto presentation_background(theme::Material material) && -> Element;

  auto presentation_background(theme::Glass glass) && -> Element;

  auto presentation_background(theme::Fill style) && -> Element;

  auto presentation_detents(
      std::vector<PresentationDetent> detents) && -> Element;

  auto presentation_detents(
      std::vector<PresentationDetent> detents,
      Binding<PresentationDetent> selection) && -> Element;

  auto presentation_drag_indicator(Visibility visibility) && -> Element;

  auto presentation_content_interaction(
      PresentationContentInteraction behavior) && -> Element;

  auto presentation_corner_radius(std::optional<float> radius) && -> Element;

  auto presentation_background_interaction(
      PresentationBackgroundInteraction interaction) && -> Element;

  auto presentation_sizing(PresentationSizing sizing) && -> Element;

  auto presentation_compact_adaptation(
      PresentationAdaptation adaptation) && -> Element;

  auto presentation_compact_adaptation(
      PresentationAdaptation horizontal,
      PresentationAdaptation vertical) && -> Element;

  auto presentation_placement(PresentationPlacement placement) && -> Element;

  template <class Content>
    requires std::invocable<const Content&> &&
             std::convertible_to<std::invoke_result_t<const Content&>, Element>
  auto presentation_background(Alignment alignment,
                               Content content) && -> Element {
    interactions_.edit().presentation_background = PresentationBackground{
        std::make_shared<const PresentationBackground::View>(
            alignment, [content = std::move(content)] {
              return Element{std::invoke(content)};
            })};
    return std::move(*this);
  }

  template <class Content>
    requires std::invocable<const Content&> &&
             std::convertible_to<std::invoke_result_t<const Content&>, Element>
  auto presentation_background(Content content) && -> Element {
    return std::move(*this).presentation_background(Alignment::center,
                                                    std::move(content));
  }

  auto inspector_column_width(float width) && -> Element;

  auto inspector_column_width(std::optional<float> minimum, float ideal,
                              std::optional<float> maximum) && -> Element;

  template <class Actions>
  auto dismissal_confirmation_dialog(std::string title, bool should_present,
                                     Actions actions) && -> Element;

  template <class Actions, class Message>
  auto dismissal_confirmation_dialog(std::string title, bool should_present,
                                     Actions actions,
                                     Message message) && -> Element;

  auto dialog_icon(Element icon) && -> Element;

  auto dialog_severity(DialogSeverity severity) && -> Element;

  static constexpr std::string_view suppression_label = "Don\u2019t ask again";

  auto dialog_suppression_toggle(Binding<bool> is_suppressed) && -> Element;

  auto dialog_suppression_toggle(std::string title,
                                 Binding<bool> is_suppressed) && -> Element;

  auto help(std::string text) && -> Element;

  auto help(std::string title, std::string body) && -> Element;

  template <class Content>
  auto inspector(Binding<bool> is_presented, Content content) && -> Element;

  template <class OnCompletion>
  auto file_importer(Binding<bool> is_presented,
                     std::vector<UTType> allowed_content_types,
                     OnCompletion on_completion) && -> Element;

  template <class OnCompletion>
  auto file_exporter(Binding<bool> is_presented,
                     std::vector<std::byte> document, UTType content_type,
                     std::string default_filename,
                     OnCompletion on_completion) && -> Element;

  template <class OnCompletion>
  auto file_mover(Binding<bool> is_presented, std::optional<FileURL> file,
                  OnCompletion on_completion) && -> Element;

  template <class OnCompletion>
  auto file_importer(Binding<bool> is_presented,
                     std::vector<UTType> allowed_content_types,
                     bool allows_multiple_selection,
                     OnCompletion on_completion) && -> Element;

  template <class Document, class OnCompletion>
  auto file_exporter(Binding<bool> is_presented,
                     std::vector<Document> documents, UTType content_type,
                     OnCompletion on_completion) && -> Element;

  template <class Item, class OnCompletion>
  auto file_exporter(Binding<bool> is_presented, std::optional<Item> item,
                     std::vector<UTType> content_types,
                     std::string default_filename,
                     OnCompletion on_completion) && -> Element;

  template <class Item, class OnCompletion>
  auto file_exporter(Binding<bool> is_presented, std::vector<Item> items,
                     std::vector<UTType> content_types,
                     OnCompletion on_completion) && -> Element;

  template <class OnCompletion>
  auto file_mover(Binding<bool> is_presented, std::vector<FileURL> files,
                  OnCompletion on_completion) && -> Element;

  auto file_dialog_confirmation_label(
      std::optional<std::string> label) && -> Element;

  auto file_dialog_customization_id(std::string id) && -> Element;

  auto file_dialog_default_directory(
      std::optional<FileURL> directory) && -> Element;

  auto file_dialog_message(std::optional<std::string> message) && -> Element;

  auto file_dialog_url_enabled(
      std::function<bool(const FileURL&)> predicate) && -> Element;

  template <class Content>
  auto sheet(Binding<bool> is_presented, Content content) && -> Element;

  template <class OnDismiss, class Content>
  auto sheet(Binding<bool> is_presented, OnDismiss on_dismiss,
             Content content) && -> Element;

  template <PresentedItem Item, class Content>
  auto sheet(Item item, Content content) && -> Element;

  template <PresentedItem Item, class OnDismiss, class Content>
  auto sheet(Item item, OnDismiss on_dismiss, Content content) && -> Element;

  template <class Content>
  auto full_screen_cover(Binding<bool> is_presented,
                         Content content) && -> Element;

  template <class OnDismiss, class Content>
  auto full_screen_cover(Binding<bool> is_presented, OnDismiss on_dismiss,
                         Content content) && -> Element;

  template <PresentedItem Item, class Content>
  auto full_screen_cover(Item item, Content content) && -> Element;

  template <PresentedItem Item, class OnDismiss, class Content>
  auto full_screen_cover(Item item, OnDismiss on_dismiss,
                         Content content) && -> Element;

  template <class Actions>
  auto alert(std::string title, Binding<bool> is_presented,
             Actions actions) && -> Element;

  template <class Actions, class Message>
  auto alert(std::string title, Binding<bool> is_presented, Actions actions,
             Message message) && -> Element;

  template <class T, class Actions>
  auto alert(std::string title, Binding<bool> is_presented,
             std::optional<T> presenting, Actions actions) && -> Element;

  template <class T, class Actions, class Message>
  auto alert(std::string title, Binding<bool> is_presented,
             std::optional<T> presenting, Actions actions,
             Message message) && -> Element;

  template <OptionalBindingSource Item, class Actions>
  auto alert(std::string title, Item item, Actions actions) && -> Element;

  template <OptionalBindingSource Item, class Actions, class Message>
  auto alert(std::string title, Item item, Actions actions,
             Message message) && -> Element;

  template <LocalizedError E, class Actions>
  auto alert(Binding<bool> is_presented, std::optional<E> error,
             Actions actions) && -> Element;

  template <LocalizedError E, class Actions, class Message>
  auto alert(Binding<bool> is_presented, std::optional<E> error,
             Actions actions, Message message) && -> Element;

  template <class T, LocalizedError E, class Actions>
  auto alert(Binding<bool> is_presented, const std::expected<T, E>& result,
             Actions actions) && -> Element;

  template <class Actions>
  auto confirmation_dialog(std::string title, Binding<bool> is_presented,
                           Visibility title_visibility,
                           Actions actions) && -> Element;

  template <class Actions, class Message>
  auto confirmation_dialog(std::string title, Binding<bool> is_presented,
                           Visibility title_visibility, Actions actions,
                           Message message) && -> Element;

  template <class T, class Actions>
  auto confirmation_dialog(std::string title, Binding<bool> is_presented,
                           Visibility title_visibility,
                           std::optional<T> presenting,
                           Actions actions) && -> Element;

  template <class T, class Actions, class Message>
  auto confirmation_dialog(std::string title, Binding<bool> is_presented,
                           Visibility title_visibility,
                           std::optional<T> presenting, Actions actions,
                           Message message) && -> Element;

  template <OptionalBindingSource Item, class Actions>
  auto confirmation_dialog(std::string title, Item item,
                           Visibility title_visibility,
                           Actions actions) && -> Element;

  template <OptionalBindingSource Item, class Actions, class Message>
  auto confirmation_dialog(std::string title, Item item,
                           Visibility title_visibility, Actions actions,
                           Message message) && -> Element;

  template <OptionalBindingSource Item, class Actions>
  auto alert(Item error, Actions actions) && -> Element;

  template <OptionalBindingSource Item, class Actions, class Message>
  auto alert(Item error, Actions actions, Message message) && -> Element;

  template <class Build>
    requires std::invocable<const Build&>
  auto alert(Binding<bool> is_presented, Build content) && -> Element;

  template <class Build>
    requires std::invocable<const Build&>
  auto action_sheet(Binding<bool> is_presented, Build content) && -> Element;

  template <class Content>
  auto popover(Binding<bool> is_presented, Content content) && -> Element;

  template <class Content>
  auto popover(Binding<bool> is_presented,
               PopoverAttachmentAnchor attachment_anchor,
               theme::Edge arrow_edge, Content content) && -> Element;

  template <PresentedItem Item, class Content>
  auto popover(Item item, Content content) && -> Element;

  template <PresentedItem Item, class Content>
  auto popover(Item item, PopoverAttachmentAnchor attachment_anchor,
               theme::Edge arrow_edge, Content content) && -> Element;

  template <class Content>
  auto toast(Binding<bool> is_presented, std::chrono::nanoseconds duration,
             Content content) && -> Element;

  template <OptionalBindingSource Item, class Content>
  auto toast(Item item, std::chrono::nanoseconds duration,
             Content content) && -> Element;

  template <class Content>
  auto toast(Binding<bool> is_presented, std::chrono::nanoseconds duration,
             ToastOptions options, Content content) && -> Element;

  template <OptionalBindingSource Item, class Content>
  auto toast(Item item, std::chrono::nanoseconds duration, ToastOptions options,
             Content content) && -> Element;

  auto navigation() const noexcept -> const NavigationDeclarations& {
    return *navigation_;
  }

  auto interactions() const noexcept -> const Interactions& {
    return *interactions_;
  }

  auto modifiers() const noexcept -> const Modifiers& { return *modifiers_; }

  auto declares(Identity preference) const noexcept -> bool {
    return std::ranges::any_of(environment_->preferences,
                               [preference](const Preference& declared) {
                                 return declared.key() == preference;
                               });
  }

  auto transition() const noexcept -> const std::optional<motion::Transition>& {
    return motion_->transition;
  }

  auto type() const noexcept -> ElementType { return content_->type(); }

  auto key() const noexcept -> const std::optional<ElementKey>& { return key_; }

  auto matched_geometry() const noexcept
      -> const std::optional<MatchedGeometry>& {
    return motion_->matched;
  }

  auto content() const noexcept -> const ElementContent& { return *content_; }

  auto children() const noexcept -> std::span<const Element> {
    return children_;
  }

 protected:
  auto edit_accessibility() -> AccessibilityAttachment&;

  auto accessibility_layer(AccessibilityAttachment::Layer layer,
                           Element content) && -> Element;

  auto declared() noexcept -> Interactions& { return interactions_.edit(); }

  explicit Element(std::unique_ptr<ElementContent> content,
                   std::vector<Element> children = {}) noexcept
      : content_(std::move(content)), children_(std::move(children)) {}

  auto edit_modifiers() noexcept -> Modifiers& { return modifiers_.edit(); }

  template <class Content>
  auto edit() noexcept -> Content& {
    return static_cast<Content&>(*content_);
  }

  void animate_transitions(motion::Animation animation) noexcept {
    motion_.edit().transition_animation = std::move(animation);
  }

  void zoom_from(MatchedGeometry::Key source) noexcept {
    motion_.edit().zoom = std::move(source);
  }

  static auto release_children(Element& element) noexcept
      -> std::vector<Element> {
    return std::exchange(element.children_, {});
  }

  static auto content_of(Element& element) noexcept -> ElementContent& {
    return *element.content_;
  }

  void replace_children(std::vector<Element> children) noexcept {
    children_ = std::move(children);
  }

  auto shader_layer() && -> Element;

  auto effect_layer() && -> Element;

  static void inherit(Element& wrapper, Element& inner) noexcept {
    wrapper.navigation_ = std::exchange(inner.navigation_, {});
    if (!inner.membership_->container_values.empty() ||
        wrapper.membership_.engaged()) {
      wrapper.membership_.edit().container_values =
          std::exchange(inner.membership_.edit().container_values, {});
    }
    if (!inner.modifiers_->layout_values.empty() ||
        wrapper.modifiers_.engaged()) {
      wrapper.modifiers_.edit().layout_values =
          std::exchange(inner.modifiers_.edit().layout_values, {});
    }
    const Interactions& carried = *inner.interactions_;
    if (carried.dismiss_disabled ||
        carried.presentation_background.has_value() ||
        carried.presentation != nullptr || wrapper.interactions_.engaged()) {
      Interactions& held = inner.interactions_.edit();
      Interactions& wrapped = wrapper.interactions_.edit();
      wrapped.dismiss_disabled = held.dismiss_disabled;
      wrapped.presentation_background = held.presentation_background;
      wrapped.presentation = held.presentation;
    }
  }

  static void inherit_identity(Element& wrapper, Element& inner) noexcept {
    wrapper.key_ = std::exchange(inner.key_, std::nullopt);
    if (inner.membership_.engaged() || wrapper.membership_.engaged()) {
      MembershipDeclarations& held = inner.membership_.edit();
      MembershipDeclarations& wrapped = wrapper.membership_.edit();
      wrapped.alias = std::exchange(held.alias, std::nullopt);
      wrapped.tag = std::exchange(held.tag, nullptr);
      wrapped.list_row = std::exchange(held.list_row, nullptr);
    }
  }

  template <class... Children>
  static auto collect(Children&&... children) -> std::vector<Element> {
    std::vector<Element> collected;
    collected.reserve(sizeof...(Children));
    const auto add = [&collected]<class Child>(Child&& child) {
      if constexpr (std::convertible_to<Child, Element>) {
        collected.emplace_back(std::forward<Child>(child));
      } else if (child.has_value()) {
        collected.emplace_back(*std::forward<Child>(child));
      }
    };
    (add(std::forward<Children>(children)), ...);
    static_cast<void>(add);
    return collected;
  }

 private:
  template <class T>
  auto restate_environment(const EnvironmentKey<T>& key,
                           std::type_identity_t<T> value) && -> Element {
    std::erase_if(environment_.edit().overrides,
                  [&key](const EnvironmentOverride& existing) {
                    return existing.key() == key.identity();
                  });
    std::erase_if(environment_.edit().transforms,
                  [&key](const EnvironmentTransform& existing) {
                    return existing.key() == key.identity();
                  });
    return std::move(*this).environment(key, std::move(value));
  }

  friend class Reconciler;
  friend class presentation::PresentationLayer;

  class AccessibilityPlaceholder;

  auto labeled_pair(AccessibilityLabeledPairRole role,
                    AccessibilityAttachment::Pairing pairing) && -> Element;

  auto linked_group(AccessibilityAttachment::Pairing pairing) && -> Element;

  auto rotor_entry(AccessibilityAttachment::Pairing pairing) && -> Element;

  auto navigation_icon(
      AccessibilityAttachment::NavigationIcon icon) && -> Element;

  auto speech(AccessibilitySpeech speech) && -> Element;

  void add_toolbar(ToolbarItem item);

  void add_toolbar(ToolbarItemGroup group);

  void add_toolbar(ToolbarSpacer spacer);

  void add_toolbar(const std::shared_ptr<const std::string>& toolbar,
                   ToolbarItem item);

  void add_toolbar(const std::shared_ptr<const std::string>& toolbar,
                   ToolbarItemGroup group);

  void add_toolbar(const std::shared_ptr<const std::string>& toolbar,
                   ToolbarSpacer spacer);

  auto submit_handlers() const -> Submission::Handlers;

  auto submit_handlers(const EnvironmentKey<Submission::Handlers>& key) const
      -> Submission::Handlers;

  auto grid_cell() noexcept -> GridCell&;

  auto attach(Gesture gesture, GesturePriority priority, GestureMask mask,
              std::string name) && -> Element;

  auto named_gesture(Gesture gesture, GesturePriority priority,
                     std::string_view name, bool is_enabled) && -> Element;

  auto attach_drag(std::vector<TransferItem> items,
                   std::optional<ElementKey> id,
                   std::optional<Namespace::ID> container) && -> Element;

  void distribute_decoration();

  void adopt_decoration(const Modifiers& row);

  auto receives_decoration() const noexcept -> bool;

  auto row_attributes() -> ListRow&;

  auto feedback(FeedbackOrigin origin,
                std::unique_ptr<FeedbackTrigger> trigger) && -> Element;

  auto container_background(
      PresentationBackground style,
      ContainerBackgroundPlacement placement) && -> Element;

  auto edit_presentation() -> PresentationOptions&;

  struct Lineage {
    using Owned = Element;

    static auto children(Element& element) noexcept -> std::vector<Element>& {
      return element.children_;
    }
  };

  struct EnvironmentDeclarations {
    std::vector<EnvironmentOverride> overrides;
    std::vector<EnvironmentTransform> transforms;
    std::vector<Preference> preferences;
    std::vector<std::shared_ptr<PreferenceObserver>> preference_observers;
  };

  struct LifecycleDeclarations {
    NodeHooks hooks;
    std::vector<NodeTask> tasks;
    std::vector<std::unique_ptr<ChangeObserver>> changes;
    TransactionEdit transaction;
  };

  struct MembershipDeclarations {
    std::optional<ElementKey> alias;
    std::shared_ptr<const TagValue> tag;
    std::shared_ptr<ListRow> list_row;
    KeyedValues container_values;
  };

  std::unique_ptr<ElementContent> content_;
  std::vector<Element> children_;
  std::optional<ElementKey> key_;
  support::Sparse<Modifiers> modifiers_;
  support::Sparse<Interactions> interactions_;
  support::Sparse<NavigationDeclarations> navigation_;
  support::Sparse<EnvironmentDeclarations> environment_;
  support::Sparse<LifecycleDeclarations> lifecycle_;
  support::Sparse<MotionDeclarations> motion_;
  support::Sparse<MembershipDeclarations> membership_;
};

template <Transferable T>
auto Element::draggable(T payload) && -> Element {
  interactions_.edit().drag =
      std::make_shared<const DragSource>(TransferItem::of(payload));
  return std::move(*this);
}

template <Transferable T, class Preview>
auto Element::draggable(T payload, Preview preview) && -> Element {
  class Previewed final : public DragSource {
   public:
    Previewed(TransferItem item, Preview build)
        : DragSource(std::move(item)), build_(std::move(build)) {}

    auto preview() const -> std::optional<Element> override {
      return Element{std::invoke(build_)};
    }

   private:
    Preview build_;
  };
  interactions_.edit().drag = std::make_shared<const Previewed>(
      TransferItem::of(payload), std::move(preview));
  return std::move(*this);
}

template <Transferable T, class Action>
auto Element::drop_destination(std::type_identity<T>, Action action,
                               DropTarget::Targeted is_targeted) && -> Element {
  std::vector<UTType> accepted;
  accepted.push_back(TransferItem::content_type<T>());
  interactions_.edit().drop = std::make_shared<DropTarget>(
      std::move(accepted),
      [action = std::move(action)](std::span<const TransferItem> items,
                                   Point location) mutable -> bool {
        std::vector<T> decoded;
        decoded.reserve(items.size());
        for (const TransferItem& item : items) {
          if (std::optional<T> value = item.template as<T>()) {
            decoded.push_back(std::move(*value));
          }
        }
        if (decoded.empty()) {
          return false;
        }
        return std::invoke(action, std::move(decoded), location);
      },
      std::move(is_targeted));
  return std::move(*this);
}

template <Transferable T, class Action>
  requires std::invocable<Action&, std::vector<T>, const DropSession&>
auto Element::drop_destination(std::type_identity<T>, bool is_enabled,
                               Action action) && -> Element {
  std::vector<UTType> accepted;
  accepted.push_back(TransferItem::content_type<T>());
  interactions_.edit().drop = std::make_shared<DropTarget>(
      std::move(accepted),
      DropTarget::Received{
          [action = std::move(action)](std::span<const TransferItem> items,
                                       const DropSession& session) mutable {
            std::vector<T> decoded;
            decoded.reserve(items.size());
            for (const TransferItem& item : items) {
              if (std::optional<T> value = item.template as<T>()) {
                decoded.push_back(std::move(*value));
              }
            }
            std::invoke(action, std::move(decoded), session);
          }},
      is_enabled);
  return std::move(*this);
}

template <Transferable Item>
  requires requires(const Item& item) { ElementKey{item.id}; }
auto Element::draggable(
    std::type_identity<Item>, std::optional<Item> item,
    std::optional<Namespace::ID> container_namespace) && -> Element {
  if (!item.has_value()) {
    return std::move(*this);
  }
  ElementKey id{item->id};
  std::vector<TransferItem> items;
  items.push_back(TransferItem::of(*item));
  return std::move(*this).attach_drag(std::move(items), std::move(id),
                                      container_namespace);
}

template <Transferable Item, class Payload>
  requires requires(const Item& item) {
    ElementKey{item.id};
  } && std::convertible_to<std::invoke_result_t<Payload&>, std::optional<Item>>
auto Element::draggable(std::type_identity<Item> type,
                        std::optional<Namespace::ID> container_namespace,
                        Payload item) && -> Element {
  return std::move(*this).draggable(
      type, std::optional<Item>{std::invoke(item)}, container_namespace);
}

template <Transferable Item, class ItemID>
auto Element::draggable(
    std::type_identity<Item>, ItemID Item::* id, std::optional<Item> item,
    std::optional<Namespace::ID> container_namespace) && -> Element {
  if (!item.has_value()) {
    return std::move(*this);
  }
  ElementKey key{(*item).*id};
  std::vector<TransferItem> items;
  items.push_back(TransferItem::of(*item));
  return std::move(*this).attach_drag(std::move(items), std::move(key),
                                      container_namespace);
}

template <Transferable Item, class ItemID, class Payload>
  requires std::convertible_to<std::invoke_result_t<Payload&>,
                               std::optional<Item>>
auto Element::draggable(std::type_identity<Item> type, ItemID Item::* id,
                        std::optional<Namespace::ID> container_namespace,
                        Payload payload) && -> Element {
  return std::move(*this).draggable(
      type, id, std::optional<Item>{std::invoke(payload)}, container_namespace);
}

template <Transferable Item, class Payload>
  requires requires(const Item& item) { ElementKey{item.id}; }
auto Element::drag_container(std::type_identity<Item>,
                             std::optional<Namespace::ID> container_namespace,
                             Payload payload) && -> Element {
  using ID = std::remove_cvref_t<decltype(std::declval<const Item&>().id)>;
  interactions_.edit().drag_container = std::make_shared<const DragContainer>(
      container_namespace,
      [payload = std::move(payload)](std::span<const ElementKey> keys) mutable {
        std::vector<ID> ids;
        for (const ElementKey& key : keys) {
          if (std::optional<ID> value = key.template as<ID>()) {
            ids.push_back(std::move(*value));
          }
        }
        std::vector<TransferItem> items;
        if constexpr (std::invocable<Payload&, std::vector<ID>>) {
          for (const Item& each : std::invoke(payload, std::move(ids))) {
            items.push_back(TransferItem::of(each));
          }
        } else {
          for (ID& each : ids) {
            std::optional<Item> produced{std::invoke(payload, std::move(each))};
            if (produced.has_value()) {
              items.push_back(TransferItem::of(*produced));
            }
          }
        }
        return items;
      });
  return std::move(*this);
}

template <Transferable Item, class ItemID, class Payload>
auto Element::drag_container(std::type_identity<Item>, ItemID Item::*,
                             std::optional<Namespace::ID> container_namespace,
                             Payload payload) && -> Element {
  interactions_.edit().drag_container = std::make_shared<const DragContainer>(
      container_namespace,
      [payload = std::move(payload)](std::span<const ElementKey> keys) mutable {
        std::vector<ItemID> ids;
        for (const ElementKey& key : keys) {
          if (std::optional<ItemID> value = key.template as<ItemID>()) {
            ids.push_back(std::move(*value));
          }
        }
        std::vector<TransferItem> items;
        if constexpr (std::invocable<Payload&, std::vector<ItemID>>) {
          for (const Item& each : std::invoke(payload, std::move(ids))) {
            items.push_back(TransferItem::of(each));
          }
        } else {
          for (ItemID& each : ids) {
            std::optional<Item> produced{std::invoke(payload, std::move(each))};
            if (produced.has_value()) {
              items.push_back(TransferItem::of(*produced));
            }
          }
        }
        return items;
      });
  return std::move(*this);
}

template <class ItemID>
auto Element::drag_container_selection(
    std::vector<ItemID> selection,
    std::optional<Namespace::ID> container_namespace) && -> Element {
  std::vector<ElementKey> keys;
  keys.reserve(selection.size());
  for (ItemID& each : selection) {
    keys.emplace_back(std::move(each));
  }
  interactions_.edit().drag_selection = std::make_shared<const DragSelection>(
      container_namespace, [keys = std::move(keys)] { return keys; });
  return std::move(*this);
}

template <class Item, class Move>
auto Element::reorder_container(std::type_identity<Item>, bool is_enabled,
                                Move move) && -> Element {
  using ID = std::remove_cvref_t<decltype(std::declval<const Item&>().id)>;
  return std::move(*this).reorder_container(
      std::type_identity<Item>{}, &Item::id,
      std::type_identity<ReorderableSingleCollectionIdentifier>{}, is_enabled,
      [move = std::move(move)](
          ReorderDifference<ID, ReorderableSingleCollectionIdentifier>
              difference) mutable {
        std::invoke(move, std::move(difference));
      });
}

template <class Item, class CollectionID, class Move>
auto Element::reorder_container(std::type_identity<Item> type,
                                std::type_identity<CollectionID> collection,
                                bool is_enabled, Move move) && -> Element {
  return std::move(*this).reorder_container(type, &Item::id, collection,
                                            is_enabled, std::move(move));
}

template <class Item, class ItemID, class Move>
auto Element::reorder_container(std::type_identity<Item> type,
                                ItemID Item::* id, bool is_enabled,
                                Move move) && -> Element {
  return std::move(*this).reorder_container(
      type, id, std::type_identity<ReorderableSingleCollectionIdentifier>{},
      is_enabled, std::move(move));
}

template <class Item, class ItemID, class CollectionID, class Move>
auto Element::reorder_container(std::type_identity<Item>, ItemID Item::*,
                                std::type_identity<CollectionID>,
                                bool is_enabled, Move move) && -> Element {
  interactions_.edit().reorder_container = true;
  std::vector<UTType> accepted;
  accepted.push_back(ReorderItem::content_type());
  interactions_.edit().drop = std::make_shared<DropTarget>(
      std::move(accepted),
      DropTarget::Received{[move = std::move(move)](
                               std::span<const TransferItem>,
                               const DropSession& session) mutable {
        const std::optional<
            typename ReorderDifference<ItemID, CollectionID>::Destination>
            destination =
                session.template reorder_destination<ItemID, CollectionID>();
        if (!destination.has_value() || !session.local_session().has_value()) {
          return;
        }
        std::invoke(
            move,
            ReorderDifference<ItemID, CollectionID>{
                session.local_session()->template dragged_item_ids<ItemID>(),
                *destination});
      }},
      is_enabled);
  return std::move(*this);
}

template <class T>
concept ViewContent =
    std::convertible_to<T, Element> ||
    std::same_as<std::remove_cvref_t<T>, std::optional<Element>>;

}
