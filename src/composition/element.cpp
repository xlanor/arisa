#include <arisa/composition/background_environment.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/decoration.hpp>
#include <arisa/composition/dialog_environment.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/composition/feedback_enablement.hpp>
#include <arisa/composition/file_dialog_environment.hpp>
#include <arisa/composition/focus_effect.hpp>
#include <arisa/composition/hover_effect_environment.hpp>
#include <arisa/composition/image_environment.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/layer_host.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/modifier_key_alternate.hpp>
#include <arisa/composition/persistent_system_overlays.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/position.hpp>
#include <arisa/composition/preferred_color_scheme.hpp>
#include <arisa/composition/redaction.hpp>
#include <arisa/composition/scroll_environment.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/submission.hpp>
#include <arisa/composition/symbol_effect_animator.hpp>
#include <arisa/composition/system_gesture_deferral.hpp>
#include <arisa/composition/text_environment.hpp>
#include <arisa/composition/text_font.hpp>
#include <arisa/composition/text_input_configuration.hpp>
#include <arisa/composition/text_input_environment.hpp>
#include <arisa/composition/tint.hpp>
#include <arisa/composition/toolbar_environment.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/user_defaults.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <string>
#include <utility>

namespace arisa::composition {

Element::~Element() {
  if (support::Dismantle<Lineage>::nested(children_)) {
    support::Dismantle<Lineage> dismantle;
    dismantle.run(children_);
  }
}

auto Element::id(ElementKey key) && -> Element {
  if (key_.has_value() && *key_ != key) {
    membership_.edit().alias = std::exchange(key_, std::move(key));
    return std::move(*this);
  }
  key_ = std::move(key);
  return std::move(*this);
}

auto Element::on_appear(std::move_only_function<void()> action) && -> Element {
  lifecycle_.edit().hooks.appear = std::move(action);
  return std::move(*this);
}

auto Element::on_disappear(
    std::move_only_function<void()> action) && -> Element {
  lifecycle_.edit().hooks.disappear = std::move(action);
  return std::move(*this);
}

auto Element::on_depart(
    std::move_only_function<void(DepartureHold)> action) && -> Element {
  lifecycle_.edit().hooks.depart = std::move(action);
  return std::move(*this);
}

auto Element::task(NodeTask::Plain body) && -> Element {
  lifecycle_.edit().tasks.emplace_back(std::nullopt, std::move(body));
  return std::move(*this);
}

auto Element::task(NodeTask::Fallible body) && -> Element {
  lifecycle_.edit().tasks.emplace_back(std::nullopt, std::move(body));
  return std::move(*this);
}

auto Element::task(ElementKey key, NodeTask::Plain body) && -> Element {
  lifecycle_.edit().tasks.emplace_back(std::move(key), std::move(body));
  return std::move(*this);
}

auto Element::task(ElementKey key, NodeTask::Fallible body) && -> Element {
  lifecycle_.edit().tasks.emplace_back(std::move(key), std::move(body));
  return std::move(*this);
}

auto Element::task(TaskOptions options, NodeTask::Plain body) && -> Element {
  lifecycle_.edit().tasks.emplace_back(std::nullopt, std::move(body),
                                       std::move(options));
  return std::move(*this);
}

auto Element::task(TaskOptions options, NodeTask::Fallible body) && -> Element {
  lifecycle_.edit().tasks.emplace_back(std::nullopt, std::move(body),
                                       std::move(options));
  return std::move(*this);
}

auto Element::task(ElementKey key, TaskOptions options,
                   NodeTask::Plain body) && -> Element {
  lifecycle_.edit().tasks.emplace_back(std::move(key), std::move(body),
                                       std::move(options));
  return std::move(*this);
}

auto Element::task(ElementKey key, TaskOptions options,
                   NodeTask::Fallible body) && -> Element {
  lifecycle_.edit().tasks.emplace_back(std::move(key), std::move(body),
                                       std::move(options));
  return std::move(*this);
}

auto Element::padding() && -> Element {
  return std::move(*this).padding(theme::space::lg);
}

auto Element::padding(theme::Space length) && -> Element {
  modifiers_.edit().padding = theme::Insets::all(length);
  return std::move(*this);
}

auto Element::padding(theme::Insets insets) && -> Element {
  modifiers_.edit().padding = insets;
  return std::move(*this);
}

auto Element::padding(theme::Edge edges, theme::Space length) && -> Element {
  modifiers_.edit().padding = modifiers_.edit()
                                  .padding.value_or(theme::Insets{})
                                  .replacing(edges, length);
  return std::move(*this);
}

auto Element::scene_padding(theme::Edge edges) && -> Element {
  return std::move(*this).scene_padding(ScenePadding::minimum, edges);
}

auto Element::scene_padding(ScenePadding padding,
                            theme::Edge edges) && -> Element {
  return std::move(*this).padding(edges, padding.length());
}

auto Element::margin(theme::Space length) && -> Element {
  modifiers_.edit().margin = theme::Insets::all(length);
  return std::move(*this);
}

auto Element::margin(theme::Insets insets) && -> Element {
  modifiers_.edit().margin = insets;
  return std::move(*this);
}

auto Element::flex_grow(float factor) && -> Element {
  modifiers_.edit().grow = std::max(factor, 0.0F);
  return std::move(*this);
}

auto Element::flex_shrink(float factor) && -> Element {
  modifiers_.edit().shrink = std::max(factor, 0.0F);
  return std::move(*this);
}

auto Element::frame(FrameSpec frame) && -> Element {
  if (!modifiers_.edit().frame.constrains() || !frame.constrains()) {
    modifiers_.edit().frame = std::move(frame);
    return std::move(*this);
  }
  Element wrapper{std::make_unique<Decoration>(frame.alignment, 0)};
  inherit(wrapper, *this);
  if (membership_.engaged()) {
    MembershipDeclarations& held = membership_.edit();
    MembershipDeclarations& wrapped = wrapper.membership_.edit();
    wrapped.tag = std::exchange(held.tag, nullptr);
    wrapped.list_row = std::exchange(held.list_row, nullptr);
  }
  if (motion_.engaged()) {
    MotionDeclarations& held = motion_.edit();
    MotionDeclarations& wrapped = wrapper.motion_.edit();
    wrapped.transition = std::exchange(held.transition, std::nullopt);
    wrapped.transition_animation =
        std::exchange(held.transition_animation, std::nullopt);
  }
  wrapper.children_ = collect(std::move(*this));
  wrapper.modifiers_.edit().frame = std::move(frame);
  return wrapper;
}

auto Element::fixed_size() && -> Element {
  return std::move(*this).fixed_size(true, true);
}

auto Element::fixed_size(bool horizontal, bool vertical) && -> Element {
  Axes axes{};
  if (horizontal) {
    axes = axes | Axes::horizontal;
  }
  if (vertical) {
    axes = axes | Axes::vertical;
  }
  modifiers_.edit().fixed_size = axes;
  return std::move(*this);
}

auto Element::layout_priority(double value) && -> Element {
  modifiers_.edit().layout_priority = value;
  return std::move(*this);
}

auto Element::position(float x, float y) && -> Element {
  return std::move(*this).position(Point{.x = x, .y = y});
}

auto Element::position(Point centre) && -> Element {
  Element wrapper{std::make_unique<Position>(centre)};
  inherit(wrapper, *this);
  wrapper.children_ = collect(std::move(*this));
  return wrapper;
}

auto Element::z_index(double value) && -> Element {
  modifiers_.edit().z_index = value;
  return std::move(*this);
}

auto Element::split_arrangement_layout_ratio(
    std::optional<float> ratio) && -> Element {
  const SplitArrangementSizing& declared =
      modifiers_.edit().layout_values.get(SplitArrangementSizing::key);
  return std::move(*this).layout_value(SplitArrangementSizing::key,
                                       declared.with_ratio(ratio));
}

auto Element::split_arrangement_layout_ratio(
    SplitArrangementSizing::Ratios ratios) && -> Element {
  const SplitArrangementSizing& declared =
      modifiers_.edit().layout_values.get(SplitArrangementSizing::key);
  return std::move(*this).layout_value(SplitArrangementSizing::key,
                                       declared.with_ratios(ratios));
}

auto Element::split_arrangement_layout_size(
    SplitArrangementSizing::Sizes sizes) && -> Element {
  const SplitArrangementSizing& declared =
      modifiers_.edit().layout_values.get(SplitArrangementSizing::key);
  return std::move(*this).layout_value(SplitArrangementSizing::key,
                                       declared.with_sizes(sizes));
}

auto Element::split_arrangement_fixed_layout_size(bool horizontal,
                                                  bool vertical) && -> Element {
  const SplitArrangementSizing& declared =
      modifiers_.edit().layout_values.get(SplitArrangementSizing::key);
  return std::move(*this).layout_value(
      SplitArrangementSizing::key, declared.with_fixed(horizontal, vertical));
}

auto Element::overlay_arrangement_edge(
    std::optional<VerticalEdge> edge) && -> Element {
  const OverlayArrangementEdges& declared =
      modifiers_.edit().layout_values.get(OverlayArrangementEdges::key);
  return std::move(*this).layout_value(OverlayArrangementEdges::key,
                                       declared.with(edge));
}

auto Element::overlay_arrangement_edge(
    std::optional<HorizontalEdge> edge) && -> Element {
  const OverlayArrangementEdges& declared =
      modifiers_.edit().layout_values.get(OverlayArrangementEdges::key);
  return std::move(*this).layout_value(OverlayArrangementEdges::key,
                                       declared.with(edge));
}

auto Element::container_relative_frame(
    ContainerRelativeFrame frame) && -> Element {
  modifiers_.edit().relative_frame = std::move(frame);
  return std::move(*this);
}

auto Element::container_relative_frame(Axes axes,
                                       Alignment alignment) && -> Element {
  modifiers_.edit().relative_frame = ContainerRelativeFrame{axes, alignment};
  return std::move(*this);
}

auto Element::container_relative_frame(Axes axes, std::uint32_t count,
                                       std::uint32_t span, theme::Space spacing,
                                       Alignment alignment) && -> Element {
  modifiers_.edit().relative_frame =
      ContainerRelativeFrame{axes, count, span, spacing, alignment};
  return std::move(*this);
}

auto Element::container_relative_frame(
    Axes axes, Alignment alignment,
    ContainerRelativeFrame::Length length) && -> Element {
  modifiers_.edit().relative_frame =
      ContainerRelativeFrame{axes, alignment, std::move(length)};
  return std::move(*this);
}

auto Element::coordinate_space(ElementKey name) && -> Element {
  interactions_.edit().coordinate_space = std::move(name);
  return std::move(*this);
}

auto Element::background(graphics::ShapeStyle style,
                         theme::Shape shape) && -> Element {
  if (const std::optional<theme::Material> material = style.material()) {
    return std::move(*this).background(*material, shape);
  }
  modifiers_.edit().background =
      Background{.fill = std::move(style), .shape = shape};
  return std::move(*this);
}

auto Element::background() && -> Element {
  return std::move(*this).background(graphics::ShapeStyle::background());
}

auto Element::background_style(graphics::ShapeStyle style) && -> Element {
  modifiers_.edit().background_style = style;
  return std::move(*this).environment(BackgroundEnvironment::style,
                                      std::optional{std::move(style)});
}

auto Element::background(theme::Material material,
                         theme::Shape shape) && -> Element {
  modifiers_.edit().material =
      MaterialBackground{.material = material, .shape = shape};
  return std::move(*this).environment(BackgroundEnvironment::material,
                                      std::optional{material});
}

auto Element::background_prominence(
    BackgroundProminence prominence) && -> Element {
  return std::move(*this).environment(BackgroundEnvironment::prominence,
                                      prominence);
}

auto Element::background(Element content) && -> Element {
  return std::move(*this).background(Alignment::center, std::move(content));
}

auto Element::background(Alignment alignment, Element content) && -> Element {
  Element wrapper{std::make_unique<Decoration>(alignment, 1)};
  inherit(wrapper, *this);
  wrapper.children_ = collect(std::move(content), std::move(*this));
  return wrapper;
}

auto Element::overlay(Element content) && -> Element {
  return std::move(*this).overlay(Alignment::center, std::move(content));
}

auto Element::overlay(Alignment alignment, Element content) && -> Element {
  Element wrapper{std::make_unique<Decoration>(alignment, 0)};
  inherit(wrapper, *this);
  wrapper.children_ = collect(std::move(*this), std::move(content));
  return wrapper;
}

auto Element::overlay(graphics::ShapeStyle style,
                      theme::Shape shape) && -> Element {
  modifiers_.edit().overlay =
      Background{.fill = std::move(style), .shape = shape};
  return std::move(*this);
}

auto Element::overlay(bool is_presented, OverlayLayer layer,
                      Element content) && -> Element {
  std::optional<Element> shown;
  if (is_presented) {
    std::vector<Element> children;
    children.push_back(std::move(content));
    OverlayInput input = OverlayInput::cooperative;
    if (layer >= OverlayLayer::toast && layer < OverlayLayer::debug) {
      input = OverlayInput::passthrough;
    } else if (layer < OverlayLayer::content) {
      input = OverlayInput::passthrough;
    } else if (layer >= OverlayLayer::sheet && layer < OverlayLayer::toast) {
      input = OverlayInput::modal;
    }
    shown.emplace(Layer(layer, input, Alignment::center, std::move(children)));
  }
  return LayerHost(std::move(*this), std::move(shown));
}

auto Element::allows_hit_testing(bool enabled) && -> Element {
  interactions_.edit().hit_testing =
      interactions_.edit().hit_testing && enabled;
  return std::move(*this);
}

class Element::AccessibilityPlaceholder final : public ElementContent {
 public:
  auto type() const noexcept -> ElementType override {
    return ElementType::of<AccessibilityPlaceholder>();
  }
};

auto Element::edit_accessibility() -> AccessibilityAttachment& {
  auto copy = interactions_.edit().accessibility != nullptr
                  ? std::make_shared<AccessibilityAttachment>(
                        *interactions_.edit().accessibility)
                  : std::make_shared<AccessibilityAttachment>();
  AccessibilityAttachment& edited = *copy;
  interactions_.edit().accessibility = std::move(copy);
  return edited;
}

auto Element::accessibility_layer(AccessibilityAttachment::Layer layer,
                                  Element content) && -> Element {
  Element hidden = std::move(content).hidden();
  hidden.edit_accessibility().layer_content = true;
  Element wrapper{std::make_unique<Decoration>(Alignment::center, 1)};
  inherit(wrapper, *this);
  wrapper.children_ = collect(std::move(hidden), std::move(*this));
  wrapper.edit_accessibility().layer = layer;
  return wrapper;
}

auto Element::accessibility_label(AccessibilityText label) && -> Element {
  edit_accessibility().label = std::move(label);
  return std::move(*this);
}

auto Element::accessibility_label(AccessibilityText label,
                                  bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().label = std::move(label);
  }
  return std::move(*this);
}

auto Element::accessibility_label(
    std::function<Element(Element label)> content) && -> Element {
  Element placeholder{std::make_unique<AccessibilityPlaceholder>()};
  placeholder.edit_accessibility().label_placeholder = true;
  Element built = content(std::move(placeholder));
  return std::move(*this).accessibility_layer(
      AccessibilityAttachment::Layer::label, std::move(built));
}

auto Element::accessibility_value(AccessibilityText value) && -> Element {
  edit_accessibility().value = std::move(value);
  return std::move(*this);
}

auto Element::accessibility_value(AccessibilityText value,
                                  bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().value = std::move(value);
  }
  return std::move(*this);
}

auto Element::accessibility_hint(AccessibilityText hint) && -> Element {
  edit_accessibility().hint = std::move(hint);
  return std::move(*this);
}

auto Element::accessibility_hint(AccessibilityText hint,
                                 bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().hint = std::move(hint);
  }
  return std::move(*this);
}

auto Element::accessibility_identifier(std::string identifier) && -> Element {
  edit_accessibility().identifier = std::move(identifier);
  return std::move(*this);
}

auto Element::accessibility_identifier(std::string identifier,
                                       bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().identifier = std::move(identifier);
  }
  return std::move(*this);
}

auto Element::accessibility_add_traits(
    AccessibilityTraits traits) && -> Element {
  AccessibilityAttachment& edited = edit_accessibility();
  edited.added = edited.added.with(traits);
  edited.removed = edited.removed.without(traits);
  return std::move(*this);
}

auto Element::accessibility_remove_traits(
    AccessibilityTraits traits) && -> Element {
  AccessibilityAttachment& edited = edit_accessibility();
  edited.removed = edited.removed.with(traits);
  edited.added = edited.added.without(traits);
  return std::move(*this);
}

auto Element::accessibility_heading(
    AccessibilityHeadingLevel level) && -> Element {
  edit_accessibility().heading = level;
  return std::move(*this);
}

auto Element::accessibility_element(
    AccessibilityChildBehavior children) && -> Element {
  edit_accessibility().children = children;
  return std::move(*this);
}

auto Element::accessibility_hidden(bool hidden) && -> Element {
  edit_accessibility().hidden = hidden;
  return std::move(*this);
}

auto Element::accessibility_hidden(bool hidden, bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().hidden = hidden;
  }
  return std::move(*this);
}

auto Element::accessibility_input_labels(
    std::vector<AccessibilityText> input_labels) && -> Element {
  edit_accessibility().input_labels = std::move(input_labels);
  return std::move(*this);
}

auto Element::accessibility_input_labels(
    std::vector<AccessibilityText> input_labels,
    bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().input_labels = std::move(input_labels);
  }
  return std::move(*this);
}

auto Element::accessibility_sort_priority(double sort_priority) && -> Element {
  edit_accessibility().sort_priority = sort_priority;
  return std::move(*this);
}

auto Element::accessibility_text_content_type(
    AccessibilityTextContentType type) && -> Element {
  edit_accessibility().text_content_type = type;
  return std::move(*this);
}

auto Element::accessibility_action(
    std::function<void()> handler) && -> Element {
  return std::move(*this).accessibility_action(
      AccessibilityActionKind::default_, std::move(handler));
}

auto Element::accessibility_action(
    AccessibilityActionKind kind, std::function<void()> handler) && -> Element {
  edit_accessibility().actions.emplace_back(std::move(kind),
                                            std::move(handler));
  return std::move(*this);
}

auto Element::accessibility_action(
    AccessibilityText name, std::function<void()> handler) && -> Element {
  return std::move(*this).accessibility_action(
      AccessibilityActionKind{std::move(name)}, std::move(handler));
}

auto Element::accessibility_action(
    std::function<void()> action,
    std::function<Element()> label) && -> Element {
  Element named =
      label()
          .accessibility_element(AccessibilityChildBehavior::combine)
          .accessibility_action(std::move(action));
  return std::move(*this).accessibility_layer(
      AccessibilityAttachment::Layer::actions, std::move(named));
}

auto Element::accessibility_actions(
    std::function<Element()> content) && -> Element {
  return std::move(*this).accessibility_actions(
      AccessibilityActionCategory::default_, std::move(content));
}

auto Element::accessibility_actions(
    AccessibilityActionCategory category,
    std::function<Element()> content) && -> Element {
  Element built = content();
  built.edit_accessibility().action_category = std::move(category);
  return std::move(*this).accessibility_layer(
      AccessibilityAttachment::Layer::actions, std::move(built));
}

auto Element::accessibility_activation_point(
    motion::UnitPoint activation_point) && -> Element {
  edit_accessibility().activation_point =
      AccessibilityAttachment::Location{.at = activation_point};
  return std::move(*this);
}

auto Element::accessibility_activation_point(motion::UnitPoint activation_point,
                                             bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().activation_point =
        AccessibilityAttachment::Location{.at = activation_point};
  }
  return std::move(*this);
}

auto Element::accessibility_activation_point(
    Point activation_point) && -> Element {
  edit_accessibility().activation_point =
      AccessibilityAttachment::Location{.at = activation_point};
  return std::move(*this);
}

auto Element::accessibility_activation_point(Point activation_point,
                                             bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().activation_point =
        AccessibilityAttachment::Location{.at = activation_point};
  }
  return std::move(*this);
}

auto Element::accessibility_adjustable_action(
    std::function<void(AccessibilityAdjustmentDirection)>
        handler) && -> Element {
  edit_accessibility().adjustable = std::make_shared<
      const std::function<void(AccessibilityAdjustmentDirection)>>(
      std::move(handler));
  return std::move(*this);
}

auto Element::accessibility_scroll_action(
    std::function<void(theme::Edge)> handler) && -> Element {
  edit_accessibility().scroll =
      std::make_shared<const std::function<void(theme::Edge)>>(
          std::move(handler));
  return std::move(*this);
}

auto Element::accessibility_zoom_action(
    std::function<void(AccessibilityZoomGestureAction)> handler) && -> Element {
  edit_accessibility().zoom = std::make_shared<
      const std::function<void(AccessibilityZoomGestureAction)>>(
      std::move(handler));
  return std::move(*this);
}

auto Element::accessibility_chart_descriptor(
    AccessibilityChartDescriptor descriptor) && -> Element {
  edit_accessibility().chart = std::move(descriptor);
  return std::move(*this);
}

auto Element::accessibility_children(
    std::function<Element()> children) && -> Element {
  return std::move(*this).accessibility_layer(
      AccessibilityAttachment::Layer::children, children());
}

auto Element::accessibility_representation(
    std::function<Element()> representation) && -> Element {
  return std::move(*this).accessibility_layer(
      AccessibilityAttachment::Layer::representation, representation());
}

auto Element::accessibility_focused(
    AccessibilityFocusBinding<bool> condition) && -> Element {
  if (condition.active()) {
    interactions_.edit().focused.push_back(FocusRequest::of(condition.focus()));
  }
  return std::move(*this);
}

auto Element::accessibility_default_focus(
    AccessibilityFocusBinding<bool> binding, bool value) && -> Element {
  if (!binding.active()) {
    return std::move(*this);
  }
  return std::move(*this).default_focus(binding.focus(), value);
}

auto Element::accessibility_direct_touch(
    bool is_direct_touch_area,
    AccessibilityDirectTouchOptions options) && -> Element {
  AccessibilityAttachment& edited = edit_accessibility();
  if (is_direct_touch_area) {
    edited.direct_touch = options;
    edited.added =
        edited.added.with(AccessibilityTrait::allows_direct_interaction);
  } else {
    edited.direct_touch.reset();
    edited.added =
        edited.added.without(AccessibilityTrait::allows_direct_interaction);
  }
  return std::move(*this);
}

auto Element::accessibility_drag_point(
    motion::UnitPoint point, AccessibilityText description) && -> Element {
  edit_accessibility().drag_points.push_back(AccessibilityAttachment::Location{
      .at = point, .description = std::move(description)});
  return std::move(*this);
}

auto Element::accessibility_drag_point(motion::UnitPoint point,
                                       AccessibilityText description,
                                       bool is_enabled) && -> Element {
  if (!is_enabled) {
    return std::move(*this);
  }
  return std::move(*this).accessibility_drag_point(point,
                                                   std::move(description));
}

auto Element::accessibility_drop_point(
    motion::UnitPoint point, AccessibilityText description) && -> Element {
  edit_accessibility().drop_points.push_back(AccessibilityAttachment::Location{
      .at = point, .description = std::move(description)});
  return std::move(*this);
}

auto Element::accessibility_drop_point(motion::UnitPoint point,
                                       AccessibilityText description,
                                       bool is_enabled) && -> Element {
  if (!is_enabled) {
    return std::move(*this);
  }
  return std::move(*this).accessibility_drop_point(point,
                                                   std::move(description));
}

auto Element::accessibility_ignores_invert_colors(bool active) && -> Element {
  edit_accessibility().ignores_invert_colors = active;
  return std::move(*this);
}

auto Element::labeled_pair(
    AccessibilityLabeledPairRole role,
    AccessibilityAttachment::Pairing pairing) && -> Element {
  edit_accessibility().labeled_pair = AccessibilityAttachment::LabeledPair{
      .role = role, .pairing = std::move(pairing)};
  return std::move(*this);
}

auto Element::linked_group(
    AccessibilityAttachment::Pairing pairing) && -> Element {
  edit_accessibility().linked_group = std::move(pairing);
  return std::move(*this);
}

auto Element::rotor_entry(
    AccessibilityAttachment::Pairing pairing) && -> Element {
  edit_accessibility().rotor_entry = std::move(pairing);
  return std::move(*this);
}

auto Element::navigation_icon(
    AccessibilityAttachment::NavigationIcon icon) && -> Element {
  edit_accessibility().navigation_icon = std::move(icon);
  return std::move(*this);
}

auto Element::accessibility_responds_to_user_interaction(
    bool responds_to_user_interaction) && -> Element {
  edit_accessibility().responds_to_user_interaction =
      responds_to_user_interaction;
  return std::move(*this);
}

auto Element::accessibility_responds_to_user_interaction(
    bool responds_to_user_interaction, bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().responds_to_user_interaction =
        responds_to_user_interaction;
  }
  return std::move(*this);
}

auto Element::accessibility_rotor(
    AccessibilityRotor::Label label,
    AccessibilityRotor::Entries entries) && -> Element {
  edit_accessibility().rotors.emplace_back(std::move(label),
                                           std::move(entries));
  return std::move(*this);
}

auto Element::accessibility_rotor(
    AccessibilityRotor::Label label,
    std::vector<AccessibilityRotorEntry::TextRange> text_ranges) && -> Element {
  return std::move(*this).accessibility_rotor(
      std::move(label),
      AccessibilityRotor::Entries{[ranges = std::move(text_ranges)] {
        std::vector<AccessibilityRotorEntry> made;
        made.reserve(ranges.size());
        for (const AccessibilityRotorEntry::TextRange& range : ranges) {
          made.emplace_back(std::nullopt, range);
        }
        return made;
      }});
}

auto Element::accessibility_scroll_status(AccessibilityText status,
                                          bool is_enabled) && -> Element {
  if (is_enabled) {
    edit_accessibility().scroll_status = std::move(status);
  }
  return std::move(*this);
}

auto Element::accessibility_shows_large_content_viewer() && -> Element {
  edit_accessibility().shows_large_content_viewer = true;
  return std::move(*this);
}

auto Element::accessibility_shows_large_content_viewer(
    std::function<Element()> large_content_view) && -> Element {
  AccessibilityAttachment& edited = edit_accessibility();
  edited.shows_large_content_viewer = true;
  edited.large_content = std::make_shared<const std::function<Element()>>(
      std::move(large_content_view));
  return std::move(*this);
}

auto Element::accessibility_custom_content(
    AccessibilityCustomContentKey key, AccessibilityText value) && -> Element {
  edit_accessibility().custom_content.push_back(
      AccessibilityAttachment::CustomContent{.key = std::move(key),
                                             .value = std::move(value)});
  return std::move(*this);
}

auto Element::speech(AccessibilitySpeech speech) && -> Element {
  AccessibilityAttachment& edited = edit_accessibility();
  edited.speech = speech.over(edited.speech.value_or(AccessibilitySpeech{}));
  return std::move(*this);
}

auto Element::speech_adjusted_pitch(double value) && -> Element {
  return std::move(*this).speech(AccessibilitySpeech{.adjusted_pitch = value});
}

auto Element::speech_always_includes_punctuation(bool value) && -> Element {
  return std::move(*this).speech(
      AccessibilitySpeech{.always_includes_punctuation = value});
}

auto Element::speech_announcements_queued(bool value) && -> Element {
  return std::move(*this).speech(
      AccessibilitySpeech{.announcements_queued = value});
}

auto Element::speech_spells_out_characters(bool value) && -> Element {
  return std::move(*this).speech(
      AccessibilitySpeech{.spells_out_characters = value});
}

auto Element::assistive_access_navigation_icon(Element icon) && -> Element {
  const AccessibilityDefaults defaults = icon.content().accessibility();
  return std::move(*this).navigation_icon(
      AccessibilityAttachment::NavigationIcon{
          .system_image = defaults.label.value_or(std::string{}),
          .resource = true});
}

auto Element::flips_for_right_to_left_layout_direction(
    bool enabled) && -> Element {
  modifiers_.edit().flips_for_right_to_left = enabled;
  return std::move(*this);
}

auto Element::layout_direction_behavior(
    LayoutDirectionBehavior behavior) && -> Element {
  modifiers_.edit().layout_direction_behavior = behavior;
  return std::move(*this);
}

auto Element::hidden() && -> Element {
  modifiers_.edit().opacity = 0.0F;
  interactions_.edit().hit_testing = false;
  return std::move(*this);
}

auto Element::exclusive_frame(bool is_presented,
                              std::vector<OverlayLayer> keep) && -> Element {
  if (is_presented) {
    interactions_.edit().exclusive_frame =
        ExclusiveFrame::Options{.keep = std::move(keep)};
  } else {
    interactions_.edit().exclusive_frame.reset();
  }
  return std::move(*this);
}

auto Element::exclusive_frame(StateHandle<bool> is_presented,
                              std::vector<OverlayLayer> keep) && -> Element {
  return std::move(*this).exclusive_frame(is_presented.get(), std::move(keep));
}

auto Element::container_background(
    theme::Fill style, ContainerBackgroundPlacement placement) && -> Element {
  return std::move(*this).container_background(PresentationBackground{style},
                                               placement);
}

auto Element::container_background(
    theme::Material material,
    ContainerBackgroundPlacement placement) && -> Element {
  return std::move(*this).container_background(PresentationBackground{material},
                                               placement);
}

auto Element::container_background(
    theme::Glass glass, ContainerBackgroundPlacement placement) && -> Element {
  return std::move(*this).container_background(PresentationBackground{glass},
                                               placement);
}

auto Element::container_background(
    PresentationBackground style,
    ContainerBackgroundPlacement placement) && -> Element {
  if (placement == ContainerBackgroundPlacement::window && style.view()) {
    std::vector<Element> children;
    children.push_back(style.view()->build());
    return LayerHost(std::move(*this),
                     Layer(OverlayLayer::base, OverlayInput::passthrough,
                           style.view()->alignment(), std::move(children)));
  }
  if (placement == ContainerBackgroundPlacement::window) {
    Element fill =
        style.clad(Layer(OverlayLayer::base, OverlayInput::passthrough,
                         Alignment::center, {}),
                   theme::Rectangle());
    return LayerHost(std::move(*this), std::move(fill));
  }
  if (placement == ContainerBackgroundPlacement::sheet) {
    interactions_.edit().presentation_background = style;
  }
  if (placement == ContainerBackgroundPlacement::navigation) {
    navigation_.edit().container_background = style;
  }
  return std::move(*this).environment(
      placement.key(), std::optional<PresentationBackground>{style});
}

auto Element::default_app_storage(UserDefaults store) && -> Element {
  return std::move(*this).environment(UserDefaults::app_storage,
                                      std::optional<UserDefaults>{store});
}

auto Element::navigation_title(std::string title) && -> Element {
  navigation_.edit().title = std::move(title);
  return std::move(*this);
}

auto Element::navigation_subtitle(std::string subtitle) && -> Element {
  navigation_.edit().chrome.set_subtitle(std::move(subtitle));
  return std::move(*this);
}

auto Element::navigation_bar_back_button_hidden(bool hides) && -> Element {
  navigation_.edit().chrome.set_back_button_hidden(hides);
  return std::move(*this);
}

auto Element::navigation_bar_title_display_mode(
    NavigationBarTitleDisplayMode mode) && -> Element {
  navigation_.edit().chrome.set_title_display_mode(mode);
  return std::move(*this);
}

auto Element::toolbar_title_display_mode(
    ToolbarTitleDisplayMode mode) && -> Element {
  navigation_.edit().chrome.set_title_display_mode(mode);
  return std::move(*this);
}

auto Element::toolbar_role(ToolbarRole role) && -> Element {
  navigation_.edit().chrome.set_role(role);
  return std::move(*this);
}

auto Element::toolbar_visibility(Visibility visibility,
                                 ToolbarPlacement bars) && -> Element {
  navigation_.edit().chrome.edit_bar(bars).visibility = visibility;
  return std::move(*this);
}

auto Element::toolbar_background(theme::ColourValue style,
                                 ToolbarPlacement bars) && -> Element {
  NavigationChrome::Bar& bar = navigation_.edit().chrome.edit_bar(bars);
  bar.background = style;
  bar.background_visibility = Visibility::visible;
  return std::move(*this);
}

auto Element::toolbar_background(Visibility visibility,
                                 ToolbarPlacement bars) && -> Element {
  navigation_.edit().chrome.edit_bar(bars).background_visibility = visibility;
  return std::move(*this);
}

auto Element::toolbar_color_scheme(std::optional<theme::ColorScheme> scheme,
                                   ToolbarPlacement bars) && -> Element {
  navigation_.edit().chrome.edit_bar(bars).color_scheme = scheme;
  return std::move(*this);
}

auto Element::toolbar_foreground_style(theme::ColourValue style,
                                       ToolbarPlacement bars) && -> Element {
  navigation_.edit().chrome.edit_bar(bars).foreground = style;
  return std::move(*this);
}

auto Element::toolbar_title_menu(
    std::function<Element()> content) && -> Element {
  add_toolbar(ToolbarItem::title_menu(std::move(content)));
  return std::move(*this);
}

auto Element::navigation_split_view_column_width(float width) && -> Element {
  environment_.edit().preferences.push_back(
      Preference::value(NavigationDeclarations::column_width_key,
                        std::optional<NavigationSplitViewColumnWidth>{
                            NavigationSplitViewColumnWidth{width}}));
  return std::move(*this);
}

auto Element::navigation_split_view_column_width(
    std::optional<float> minimum, float ideal,
    std::optional<float> maximum) && -> Element {
  environment_.edit().preferences.push_back(Preference::value(
      NavigationDeclarations::column_width_key,
      std::optional<NavigationSplitViewColumnWidth>{
          NavigationSplitViewColumnWidth{minimum, ideal, maximum}}));
  return std::move(*this);
}

auto Element::navigation_split_view_style(
    NavigationSplitViewStyle style) && -> Element {
  return std::move(*this).environment(NavigationDeclarations::split_view_style,
                                      style);
}

auto Element::interactive_dismiss_disabled(bool disabled) && -> Element {
  interactions_.edit().dismiss_disabled = disabled;
  return std::move(*this);
}

auto Element::presentation_background(theme::Material material) && -> Element {
  interactions_.edit().presentation_background =
      PresentationBackground{material};
  return std::move(*this);
}

auto Element::presentation_background(theme::Glass glass) && -> Element {
  interactions_.edit().presentation_background = PresentationBackground{glass};
  return std::move(*this);
}

auto Element::presentation_background(theme::Fill style) && -> Element {
  interactions_.edit().presentation_background = PresentationBackground{style};
  return std::move(*this);
}

auto Element::edit_presentation() -> PresentationOptions& {
  std::shared_ptr<PresentationOptions> edited =
      interactions_.edit().presentation != nullptr
          ? std::make_shared<PresentationOptions>(
                *interactions_.edit().presentation)
          : std::make_shared<PresentationOptions>();
  PresentationOptions& options = *edited;
  interactions_.edit().presentation = std::move(edited);
  return options;
}

auto Element::presentation_detents(
    std::vector<PresentationDetent> detents) && -> Element {
  edit_presentation().detents = std::move(detents);
  return std::move(*this);
}

auto Element::presentation_detents(
    std::vector<PresentationDetent> detents,
    Binding<PresentationDetent> selection) && -> Element {
  PresentationOptions& options = edit_presentation();
  options.detents = std::move(detents);
  options.selection.emplace(std::move(selection));
  return std::move(*this);
}

auto Element::presentation_drag_indicator(Visibility visibility) && -> Element {
  edit_presentation().drag_indicator = visibility;
  return std::move(*this);
}

auto Element::presentation_content_interaction(
    PresentationContentInteraction behavior) && -> Element {
  edit_presentation().content_interaction = behavior;
  return std::move(*this);
}

auto Element::presentation_corner_radius(
    std::optional<float> radius) && -> Element {
  edit_presentation().corner_radius = radius;
  return std::move(*this);
}

auto Element::presentation_background_interaction(
    PresentationBackgroundInteraction interaction) && -> Element {
  edit_presentation().background_interaction = interaction;
  return std::move(*this);
}

auto Element::presentation_sizing(PresentationSizing sizing) && -> Element {
  edit_presentation().sizing = sizing;
  return std::move(*this);
}

auto Element::presentation_compact_adaptation(
    PresentationAdaptation adaptation) && -> Element {
  edit_presentation().compact_adaptation = adaptation;
  return std::move(*this);
}

auto Element::presentation_compact_adaptation(
    PresentationAdaptation horizontal,
    PresentationAdaptation vertical) && -> Element {
  edit_presentation().compact_adaptation = horizontal;
  edit_presentation().vertical_adaptation = vertical;
  return std::move(*this);
}

auto Element::presentation_placement(
    PresentationPlacement placement) && -> Element {
  edit_presentation().placement = placement;
  return std::move(*this);
}

auto Element::inspector_column_width(float width) && -> Element {
  edit_presentation().inspector_width = InspectorColumnWidth{width};
  return std::move(*this);
}

auto Element::inspector_column_width(
    std::optional<float> minimum, float ideal,
    std::optional<float> maximum) && -> Element {
  edit_presentation().inspector_width =
      InspectorColumnWidth{minimum, ideal, maximum};
  return std::move(*this);
}

auto Element::dialog_icon(Element icon) && -> Element {
  const auto held = std::make_shared<Element>(std::move(icon));
  edit_presentation().dialog_icon = held;
  return std::move(*this).environment(DialogEnvironment::icon, held);
}

auto Element::dialog_severity(DialogSeverity severity) && -> Element {
  edit_presentation().dialog_severity = severity;
  return std::move(*this).environment(DialogEnvironment::severity, severity);
}

auto Element::dialog_suppression_toggle(
    Binding<bool> is_suppressed) && -> Element {
  return std::move(*this).dialog_suppression_toggle(
      std::string{suppression_label}, std::move(is_suppressed));
}

auto Element::dialog_suppression_toggle(
    std::string title, Binding<bool> is_suppressed) && -> Element {
  PresentationOptions::Suppression made{
      .title = std::move(title), .is_suppressed = std::move(is_suppressed)};
  edit_presentation().suppression.emplace(made);
  return std::move(*this).environment(
      DialogEnvironment::suppression,
      std::optional<PresentationOptions::Suppression>{std::move(made)});
}

auto Element::clip_shape(theme::Shape shape) && -> Element {
  if (shape.form == theme::ShapeForm::circle || shape.elliptical()) {
    modifiers_.edit().corners.reset();
    modifiers_.edit().clip = true;
    modifiers_.edit().clip_path = graphics::AnyShape{shape};
    modifiers_.edit().clip_fill = graphics::FillStyle{};
    return std::move(*this);
  }
  modifiers_.edit().corners = shape;
  modifiers_.edit().clip = true;
  modifiers_.edit().clip_path.reset();
  return std::move(*this);
}

auto Element::clip_shape(graphics::AnyShape shape,
                         graphics::FillStyle style) && -> Element {
  if (const std::optional<theme::Shape> leaf = shape.leaf();
      leaf.has_value() && graphics::AnyShape{*leaf} == shape &&
      leaf->form != theme::ShapeForm::circle && !leaf->elliptical()) {
    return std::move(*this).clip_shape(*leaf);
  }
  modifiers_.edit().corners.reset();
  modifiers_.edit().clip = true;
  modifiers_.edit().clip_path = std::move(shape);
  modifiers_.edit().clip_fill = style;
  return std::move(*this);
}

auto Element::clipped(bool antialiased) && -> Element {
  modifiers_.edit().clip_fill.antialiased = antialiased;
  return std::move(*this).clip_shape(theme::Shape::rectangle());
}

auto Element::container_corner_offset(theme::Edge edges,
                                      bool size_to_fit) && -> Element {
  modifiers_.edit().corner_offset =
      CornerOffset{.edges = edges, .size_to_fit = size_to_fit};
  return std::move(*this);
}

auto Element::container_shape(theme::Shape shape) && -> Element {
  modifiers_.edit().container_shape = shape;
  return std::move(*this);
}

auto Element::border(theme::ColourValue colour,
                     theme::Stroke width) && -> Element {
  modifiers_.edit().border = Border{.width = width, .colour = colour};
  return std::move(*this);
}

auto Element::shadow(theme::ColourValue colour, float radius, float x,
                     float y) && -> Element {
  modifiers_.edit().drop_shadow = DropShadow{
      .colour = colour, .radius = std::max(radius, 0.0F), .x = x, .y = y};
  return std::move(*this);
}

auto Element::shadow(theme::Elevation elevation) && -> Element {
  modifiers_.edit().shadow = elevation;
  return std::move(*this);
}

auto Element::foreground_style(graphics::ShapeStyle style) && -> Element {
  modifiers_.edit().foreground.emplace(std::move(style));
  return std::move(*this);
}

auto Element::foreground_style(graphics::ShapeStyle primary,
                               graphics::ShapeStyle secondary) && -> Element {
  modifiers_.edit().foreground.emplace(std::move(primary),
                                       std::move(secondary));
  return std::move(*this);
}

auto Element::foreground_style(graphics::ShapeStyle primary,
                               graphics::ShapeStyle secondary,
                               graphics::ShapeStyle tertiary) && -> Element {
  modifiers_.edit().foreground.emplace(std::move(primary), std::move(secondary),
                                       std::move(tertiary));
  return std::move(*this);
}

auto Element::opacity(float value) && -> Element {
  modifiers_.edit().opacity *= std::clamp(value, 0.0F, 1.0F);
  return std::move(*this);
}

auto Element::brightness(double amount) && -> Element {
  modifiers_.edit().graphics.colour.emplace_back(
      graphics::ColourMatrix::brightness(amount));
  return std::move(*this);
}

auto Element::contrast(double amount) && -> Element {
  modifiers_.edit().graphics.colour.emplace_back(
      graphics::ColourMatrix::contrast(amount));
  return std::move(*this);
}

auto Element::saturation(double amount) && -> Element {
  modifiers_.edit().graphics.colour.emplace_back(
      graphics::ColourMatrix::saturation(amount));
  return std::move(*this);
}

auto Element::grayscale(double amount) && -> Element {
  modifiers_.edit().graphics.colour.emplace_back(
      graphics::ColourMatrix::grayscale(amount));
  return std::move(*this);
}

auto Element::hue_rotation(motion::Angle angle) && -> Element {
  modifiers_.edit().graphics.colour.emplace_back(
      graphics::ColourMatrix::hue_rotation(angle));
  return std::move(*this);
}

auto Element::color_invert(bool inverted) && -> Element {
  if (inverted) {
    modifiers_.edit().graphics.colour.emplace_back(
        graphics::ColourMatrix::inverted());
  }
  return std::move(*this);
}

auto Element::color_multiply(theme::ColourValue colour) && -> Element {
  modifiers_.edit().graphics.colour.emplace_back(colour);
  return std::move(*this);
}

auto Element::luminance_to_alpha() && -> Element {
  modifiers_.edit().graphics.colour.emplace_back(
      graphics::ColourMatrix::luminance_to_alpha());
  return std::move(*this);
}

auto Element::colour_matrix(graphics::ColourMatrix matrix) && -> Element {
  GraphicEffects& held = modifiers_.edit().graphics;
  if (!held.colour.empty() &&
      (held.premultiplied_colour || held.linear_colour)) {
    return std::move(*this).effect_layer().colour_matrix(matrix);
  }
  held.colour.emplace_back(matrix);
  return std::move(*this);
}

auto Element::colour_matrix(graphics::ColourMatrix matrix, bool premultiplied,
                            bool linear) && -> Element {
  GraphicEffects& held = modifiers_.edit().graphics;
  if (!held.colour.empty() && (held.premultiplied_colour != premultiplied ||
                               held.linear_colour != linear)) {
    Element layered = std::move(*this).effect_layer();
    return std::move(layered).colour_matrix(matrix, premultiplied, linear);
  }
  held.colour.emplace_back(matrix);
  held.premultiplied_colour = premultiplied;
  held.linear_colour = linear;
  return std::move(*this);
}

auto Element::effect_layer() && -> Element {
  Element wrapper{std::make_unique<Decoration>(Alignment::center, 0)};
  inherit(wrapper, *this);
  wrapper.children_ = collect(std::move(*this));
  return wrapper;
}

auto Element::blur(float radius, bool opaque) && -> Element {
  const float added = std::max(radius, 0.0F);
  const float held = modifiers_.edit().graphics.blur_radius;
  modifiers_.edit().graphics.blur_radius =
      std::sqrt((held * held) + (added * added));
  modifiers_.edit().graphics.blur_opaque =
      modifiers_.edit().graphics.blur_opaque || opaque;
  modifiers_.edit().graphics.blurs = true;
  return std::move(*this);
}

auto Element::blend_mode(graphics::BlendMode mode) && -> Element {
  modifiers_.edit().graphics.blend = mode;
  return std::move(*this);
}

auto Element::compositing_group() && -> Element {
  modifiers_.edit().graphics.group = true;
  return std::move(*this);
}

auto Element::drawing_group([[maybe_unused]] bool opaque) && -> Element {
  modifiers_.edit().graphics.group = true;
  return std::move(*this);
}

auto Element::rotation_3d_effect(motion::Angle angle,
                                 graphics::Rotation3D::Axis axis,
                                 motion::UnitPoint anchor, float anchor_z,
                                 float perspective) && -> Element {
  modifiers_.edit().graphics.projections.emplace_back(
      graphics::Rotation3D{angle, axis, anchor, anchor_z, perspective});
  return std::move(*this);
}

auto Element::perspective_rotation_effect(motion::Angle angle,
                                          graphics::Rotation3D::Axis axis,
                                          motion::UnitPoint anchor,
                                          float anchor_z,
                                          float perspective) && -> Element {
  return std::move(*this).rotation_3d_effect(angle, axis, anchor, anchor_z,
                                             perspective);
}

auto Element::transform_effect(
    graphics::AffineTransform transform) && -> Element {
  modifiers_.edit().graphics.projections.emplace_back(
      graphics::ProjectionTransform{transform});
  return std::move(*this);
}

auto Element::projection_effect(
    graphics::ProjectionTransform transform) && -> Element {
  modifiers_.edit().graphics.projections.emplace_back(transform);
  return std::move(*this);
}

auto Element::shader_layer() && -> Element {
  if (!modifiers_.edit().graphics.shader.has_value()) {
    return std::move(*this);
  }
  return std::move(*this).effect_layer();
}

auto Element::color_effect(graphics::Shader shader,
                           bool is_enabled) && -> Element {
  if (is_enabled) {
    Element layered = std::move(*this).shader_layer();
    layered.modifiers_.edit().graphics.shader = std::move(shader);
    layered.modifiers_.edit().graphics.sample_offset = Size{};
    return layered;
  }
  return std::move(*this);
}

auto Element::distortion_effect(graphics::Shader shader, Size max_sample_offset,
                                bool is_enabled) && -> Element {
  if (!is_enabled) {
    return std::move(*this);
  }
  Element layered = std::move(*this).shader_layer();
  layered.modifiers_.edit().graphics.shader = std::move(shader);
  layered.modifiers_.edit().graphics.sample_offset = max_sample_offset;
  return layered;
}

auto Element::layer_effect(graphics::Shader shader, Size max_sample_offset,
                           bool is_enabled) && -> Element {
  if (!is_enabled) {
    return std::move(*this);
  }
  Element layered = std::move(*this).shader_layer();
  layered.modifiers_.edit().graphics.shader = std::move(shader);
  layered.modifiers_.edit().graphics.sample_offset = max_sample_offset;
  return layered;
}

auto Element::mask(Element content) && -> Element {
  return std::move(*this).mask(Alignment::center, std::move(content));
}

auto Element::mask(Alignment alignment, Element content) && -> Element {
  Element wrapper{std::make_unique<Decoration>(alignment, 0, true)};
  inherit(wrapper, *this);
  wrapper.children_ =
      collect(std::move(*this), std::move(content).allows_hit_testing(false));
  return wrapper;
}

auto Element::reverse_mask(Alignment alignment, Element content) && -> Element {
  Element wrapper{std::make_unique<Decoration>(alignment, 0, true, true)};
  inherit(wrapper, *this);
  wrapper.children_ =
      collect(std::move(*this), std::move(content).allows_hit_testing(false));
  return wrapper;
}

auto Element::scale_effect(float scale,
                           motion::UnitPoint anchor) && -> Element {
  return std::move(*this).scale_effect(scale, scale, anchor);
}

auto Element::scale_effect(float x, float y,
                           motion::UnitPoint anchor) && -> Element {
  modifiers_.edit().effects.scale_x *= x;
  modifiers_.edit().effects.scale_y *= y;
  modifiers_.edit().effects.scale_anchor = anchor;
  return std::move(*this);
}

auto Element::offset(float x, float y) && -> Element {
  modifiers_.edit().effects.offset_x += x;
  modifiers_.edit().effects.offset_y += y;
  return std::move(*this);
}

auto Element::rotation_effect(motion::Angle angle,
                              motion::UnitPoint anchor) && -> Element {
  modifiers_.edit().effects.rotation =
      modifiers_.edit().effects.rotation + angle;
  modifiers_.edit().effects.rotation_anchor = anchor;
  return std::move(*this);
}

auto Element::transition(motion::Transition transition) && -> Element {
  motion_.edit().transition = transition;
  return std::move(*this);
}

auto Element::matched_geometry_effect(
    ElementKey id, Namespace::ID space,
    motion::MatchedGeometryProperties properties, motion::UnitPoint anchor,
    bool is_source) && -> Element {
  motion_.edit().matched = MatchedGeometry{
      .key = MatchedGeometry::Key{.space = space, .id = std::move(id)},
      .properties = properties,
      .anchor = anchor,
      .is_source = is_source};
  return std::move(*this);
}

auto Element::content_transition(
    motion::ContentTransition transition) && -> Element {
  modifiers_.edit().content_transition = transition;
  return std::move(*this).environment(BackgroundEnvironment::content_transition,
                                      transition);
}

auto Element::hover_effect(CustomHoverEffect effect,
                           bool is_enabled) && -> Element {
  return std::move(*this).hover_effect(std::move(effect), std::nullopt,
                                       is_enabled);
}

auto Element::hover_effect(CustomHoverEffect effect,
                           std::optional<HoverEffectGroup> in,
                           bool is_enabled) && -> Element {
  if (!is_enabled) {
    modifiers_.edit().hover.reset();
    return std::move(*this);
  }
  modifiers_.edit().hover =
      CustomHoverEffect::empty().hover_effect(std::move(effect), std::move(in));
  return std::move(*this);
}

auto Element::hover_effect(CustomHoverEffect::Body body) && -> Element {
  return std::move(*this).hover_effect(std::nullopt, true, std::move(body));
}

auto Element::hover_effect(std::optional<HoverEffectGroup> in, bool is_enabled,
                           CustomHoverEffect::Body body) && -> Element {
  if (!is_enabled) {
    modifiers_.edit().hover.reset();
    return std::move(*this);
  }
  modifiers_.edit().hover = CustomHoverEffect::empty().hover_effect(
      std::move(in), true, std::move(body));
  return std::move(*this);
}

auto Element::default_hover_effect(
    std::optional<CustomHoverEffect> effect) && -> Element {
  return std::move(*this).environment(
      HoverEffectEnvironment::default_hover_effect, std::move(effect));
}

auto Element::hover_effect_disabled(bool disabled) && -> Element {
  if (!disabled) {
    return std::move(*this);
  }
  return std::move(*this).environment(
      HoverEffectEnvironment::is_hover_effect_enabled, false);
}

auto Element::hover_effect_group() && -> Element {
  interactions_.edit().hover_container = Interactions::HoverContainer{
      .kind = Interactions::HoverContainer::Kind::implicit,
      .group = std::nullopt};
  return std::move(*this);
}

auto Element::hover_effect_group(
    std::optional<HoverEffectGroup> group) && -> Element {
  interactions_.edit().hover_container = Interactions::HoverContainer{
      .kind = group.has_value() ? Interactions::HoverContainer::Kind::named
                                : Interactions::HoverContainer::Kind::cleared,
      .group = std::move(group)};
  return std::move(*this);
}

auto Element::hover_effect_group(
    std::optional<std::string> id, Namespace::ID space,
    HoverEffectGroup::Behavior behavior) && -> Element {
  return std::move(*this).hover_effect_group(
      HoverEffectGroup{std::move(id), space, behavior});
}

auto Element::on_hover(
    std::move_only_function<void(bool)> action) && -> Element {
  interactions_.edit().hover =
      std::make_shared<std::move_only_function<void(bool)>>(std::move(action));
  return std::move(*this);
}

auto Element::on_continuous_hover(
    std::move_only_function<void(HoverPhase)> action) && -> Element {
  return std::move(*this).on_continuous_hover(CoordinateSpace::local,
                                              std::move(action));
}

auto Element::on_continuous_hover(
    CoordinateSpace coordinate_space,
    std::move_only_function<void(HoverPhase)> action) && -> Element {
  interactions_.edit().continuous_hover =
      std::make_shared<Interactions::ContinuousHover>(
          Interactions::ContinuousHover{.space = std::move(coordinate_space),
                                        .action = std::move(action)});
  return std::move(*this);
}

auto Element::pointer_style(std::optional<PointerStyle> style) && -> Element {
  interactions_.edit().pointer_style = style.value_or(PointerStyle::default_);
  return std::move(*this);
}

auto Element::pointer_visibility(Visibility visibility) && -> Element {
  interactions_.edit().pointer_visibility = visibility;
  return std::move(*this);
}

auto Element::motion_path(motion::MotionPath path,
                          double progress) && -> Element {
  modifiers_.edit().path =
      PathEffect{.path = std::move(path), .progress = progress};
  return std::move(*this);
}

auto Element::focusable(bool is_focusable) && -> Element {
  interactions_.edit().focusable = is_focusable;
  return std::move(*this);
}

auto Element::focusable(bool is_focusable,
                        FocusInteractions interactions) && -> Element {
  interactions_.edit().focusable = is_focusable;
  interactions_.edit().focus_interactions = interactions;
  return std::move(*this);
}

auto Element::focus_fallback() && -> Element {
  interactions_.edit().focusable = true;
  interactions_.edit().focus_fallback = true;
  return std::move(*this);
}

auto Element::focus_ring_inset(float points) && -> Element {
  interactions_.edit().focus_ring_inset = std::max(points, 0.0F);
  return std::move(*this);
}

auto Element::focused(FocusBinding<bool> condition) && -> Element {
  interactions_.edit().focused.push_back(
      FocusRequest::of(std::move(condition)));
  return std::move(*this);
}

auto Element::focused(std::unique_ptr<FocusRequest> request) && -> Element {
  interactions_.edit().focused.push_back(std::move(request));
  return std::move(*this);
}

auto Element::default_focus(FocusBinding<bool> binding,
                            bool value) && -> Element {
  return std::move(*this).default_focus(
      std::move(binding), value, DefaultFocusEvaluationPriority::automatic);
}

auto Element::default_focus(
    FocusBinding<bool> binding, bool value,
    DefaultFocusEvaluationPriority priority) && -> Element {
  interactions_.edit().default_focus.push_back(Interactions::DefaultFocus{
      .request = FocusRequest::of(std::move(binding), value),
      .priority = priority});
  return std::move(*this);
}

auto Element::prefers_default_focus(Namespace::ID in) && -> Element {
  return std::move(*this).prefers_default_focus(true, in);
}

auto Element::prefers_default_focus(bool prefers,
                                    Namespace::ID in) && -> Element {
  if (prefers) {
    interactions_.edit().prefers_default_focus = in;
  } else {
    interactions_.edit().prefers_default_focus.reset();
  }
  return std::move(*this);
}

auto Element::focus_scope(Namespace::ID id,
                          FocusScopeBehavior behavior) && -> Element {
  interactions_.edit().focus_scope =
      Interactions::Scope{.id = id, .behavior = behavior};
  return std::move(*this);
}

auto Element::focus_section() && -> Element {
  interactions_.edit().focus_section = true;
  return std::move(*this);
}

auto Element::disabled(bool is_disabled) && -> Element {
  interactions_.edit().disabled = interactions_.edit().disabled || is_disabled;
  if (!is_disabled) {
    return std::move(*this);
  }
  return std::move(*this).environment(Enablement::is_enabled, false);
}

auto Element::on_scroll_visibility_change(
    std::move_only_function<void(bool)> action) && -> Element {
  return std::move(*this).on_scroll_visibility_change(0.5, std::move(action));
}

auto Element::on_scroll_visibility_change(
    double threshold,
    std::move_only_function<void(bool)> action) && -> Element {
  interactions_.edit().scrolling.visibility_observers.push_back(
      std::make_shared<ScrollVisibilityObserver>(threshold, std::move(action)));
  return std::move(*this);
}

auto Element::refresh_trigger(
    std::move_only_function<void()> trigger) && -> Element {
  interactions_.edit().scrolling.refresh =
      std::make_shared<std::move_only_function<void()>>(std::move(trigger));
  return std::move(*this);
}

auto Element::ignores_safe_area(theme::Edge edges) && -> Element {
  return std::move(*this).ignores_safe_area(SafeAreaRegions::all, edges);
}

auto Element::ignores_safe_area(SafeAreaRegions regions,
                                theme::Edge edges) && -> Element {
  return std::move(*this).ignores_safe_area(regions, edges, std::nullopt);
}

auto Element::ignores_safe_area(
    SafeAreaRegions regions, theme::Edge edges,
    std::optional<Alignment> alignment) && -> Element {
  interactions_.edit().scrolling.ignores_safe_area = Scrolling::IgnoredSafeArea{
      .regions = regions, .edges = edges, .alignment = alignment};
  return std::move(*this);
}

auto Element::scaffold_bar_placement(theme::Edge edge,
                                     BarPlacement placement) && -> Element {
  if (edge == theme::Edge::top) {
    return std::move(*this).environment(BarPlacements::header,
                                        std::optional<BarPlacement>{placement});
  }
  return std::move(*this).environment(BarPlacements::footer,
                                      std::optional<BarPlacement>{placement});
}

auto Element::safe_area_bar(theme::Edge edge) && -> Element {
  interactions_.edit().scrolling.safe_area_bar = edge;
  return std::move(*this);
}

auto Element::focus_effect_disabled(bool disabled) && -> Element {
  return std::move(*this).environment(FocusEffect::is_focus_effect_enabled,
                                      !disabled);
}

auto Element::sensory_feedback_disabled(bool disabled) && -> Element {
  return std::move(*this).environment(
      FeedbackEnablement::is_sensory_feedback_enabled, !disabled);
}

auto Element::feedback(FeedbackOrigin origin,
                       std::unique_ptr<FeedbackTrigger> trigger) && -> Element {
  interactions_.edit().feedback.push_back(
      Interactions::Feedback{.origin = origin, .trigger = std::move(trigger)});
  return std::move(*this);
}

auto Element::tint(std::optional<graphics::ShapeStyle> style) && -> Element {
  return std::move(*this).environment(Tint::key, std::move(style));
}

auto Element::preferred_color_scheme(
    std::optional<theme::ColorScheme> scheme) && -> Element {
  return std::move(*this).preference(PreferredColorScheme::key, scheme);
}

auto Element::persistent_system_overlays(Visibility visibility) && -> Element {
  return std::move(*this).preference(PersistentSystemOverlays::key, visibility);
}

auto Element::defers_system_gestures(theme::Edge edges) && -> Element {
  return std::move(*this).preference(SystemGestureDeferral::key,
                                     SystemGestureDeferral::bits(edges));
}

auto Element::on_submit(std::move_only_function<void()> action) && -> Element {
  return std::move(*this).on_submit(SubmitTriggers::text, std::move(action));
}

auto Element::on_submit(SubmitTriggers triggers,
                        std::move_only_function<void()> action) && -> Element {
  const bool text = triggers.contains(SubmitTriggers::text);
  const bool search = triggers.contains(SubmitTriggers::search);
  if (text && search) {
    auto shared =
        std::make_shared<std::move_only_function<void()>>(std::move(action));
    Element first = std::move(*this).on_submit(SubmitTriggers::text,
                                               [shared] { (*shared)(); });
    return std::move(first).on_submit(SubmitTriggers::search,
                                      [shared] { (*shared)(); });
  }
  if (!text && !search) {
    return std::move(*this);
  }
  const EnvironmentKey<Submission::Handlers>& key =
      text ? Submission::handlers : Submission::search_handlers;
  Submission::Handlers handlers = submit_handlers(key);
  handlers.add(std::move(action));
  return std::move(*this).environment(key, std::move(handlers));
}

auto Element::submit_scope(bool is_blocking) && -> Element {
  if (!is_blocking) {
    return std::move(*this);
  }
  Submission::Handlers handlers = submit_handlers();
  handlers.scope();
  Submission::Handlers searches = submit_handlers(Submission::search_handlers);
  searches.scope();
  return std::move(*this)
      .environment(Submission::handlers, std::move(handlers))
      .environment(Submission::search_handlers, std::move(searches));
}

auto Element::submit_handlers() const -> Submission::Handlers {
  return submit_handlers(Submission::handlers);
}

auto Element::submit_handlers(const EnvironmentKey<Submission::Handlers>& key)
    const -> Submission::Handlers {
  for (const EnvironmentOverride& entry : environment_->overrides) {
    if (entry.key() == key.identity()) {
      return static_cast<
                 const EnvironmentOverride::Typed<Submission::Handlers>&>(
                 entry.value())
          .get();
    }
  }
  return Submission::Handlers{};
}

auto Element::submit_label(provider::SubmitLabel label) && -> Element {
  return std::move(*this).environment(Submission::label, label);
}

auto Element::keyboard_type(provider::KeyboardType type) && -> Element {
  return std::move(*this).environment(TextInputConfiguration::keyboard_type,
                                      type);
}

auto Element::text_content_type(
    std::optional<provider::TextContentType> type) && -> Element {
  return std::move(*this).environment(TextInputConfiguration::text_content_type,
                                      type);
}

auto Element::text_input_autocapitalization(
    std::optional<provider::TextInputAutocapitalization> style) && -> Element {
  return std::move(*this).environment(
      TextInputConfiguration::autocapitalization, style);
}

auto Element::autocorrection_disabled(bool disable) && -> Element {
  return std::move(*this).environment(
      TextInputConfiguration::autocorrection_disabled,
      std::optional<bool>{disable});
}

auto Element::font(theme::Font value) && -> Element {
  return std::move(*this).environment(TextFont::font,
                                      std::optional<theme::Font>{value});
}

auto Element::multiline_text_alignment(TextAlignment alignment) && -> Element {
  return std::move(*this).environment(TextEnvironment::multiline_text_alignment,
                                      alignment);
}

auto Element::line_limit(std::optional<std::size_t> lines) && -> Element {
  return std::move(*this).environment(TextEnvironment::line_limit,
                                      LineLimit{.lines = lines});
}

auto Element::line_limit(std::size_t lines, bool reserves_space) && -> Element {
  return std::move(*this).environment(
      TextEnvironment::line_limit,
      LineLimit{.lines = lines, .reserves_space = reserves_space});
}

auto Element::line_limit(LinesBetween lines) && -> Element {
  return std::move(*this).environment(
      TextEnvironment::line_limit,
      LineLimit{.lines = std::max(lines.minimum, lines.maximum),
                .reserves_space = lines.minimum > 0,
                .minimum = lines.minimum});
}

auto Element::line_limit(LinesAtLeast lines) && -> Element {
  return std::move(*this).environment(
      TextEnvironment::line_limit,
      LineLimit{.lines = std::nullopt,
                .reserves_space = lines.minimum > 0,
                .minimum = lines.minimum});
}

auto Element::line_height(
    std::optional<text::LineHeight> height) && -> Element {
  return std::move(*this).environment(TextEnvironment::line_height, height);
}

auto Element::typesetting_language(text::TypesettingLanguage language,
                                   bool is_enabled) && -> Element {
  if (!is_enabled) {
    return std::move(*this);
  }
  return std::move(*this).environment(TextEnvironment::typesetting_language,
                                      language.language());
}

auto Element::text_variant(TextVariantPreference preference) && -> Element {
  return std::move(*this).environment(TextEnvironment::text_variant,
                                      preference);
}

auto Element::text_selection_affinity(
    TextSelectionAffinity affinity) && -> Element {
  return std::move(*this).environment(TextEnvironment::selection_affinity,
                                      affinity);
}

auto Element::text_input_formatting_control_visibility(
    Visibility visibility,
    TextInputFormattingControlPlacement::Set placements) && -> Element {
  return std::move(*this).transform_environment(
      TextInputEnvironment::formatting_controls,
      [visibility,
       placements](TextInputEnvironment::FormattingControls& controls) {
        controls = controls.with(visibility, placements);
      });
}

auto Element::find_navigator(Binding<bool> is_presented) && -> Element {
  return std::move(*this).transform_environment(
      TextInputEnvironment::find_context,
      [is_presented =
           std::move(is_presented)](std::optional<FindContext>& context) {
        const bool replace = context.has_value() && context->supports_replace;
        context = FindContext{.is_presented = is_presented,
                              .supports_replace = replace};
      });
}

auto Element::find_disabled(bool is_disabled) && -> Element {
  return std::move(*this).environment(TextInputEnvironment::find_disabled,
                                      is_disabled);
}

auto Element::replace_disabled(bool is_disabled) && -> Element {
  return std::move(*this).environment(TextInputEnvironment::replace_disabled,
                                      is_disabled);
}

auto Element::search_selection(
    Binding<std::optional<TextSelection>> selection) && -> Element {
  return std::move(*this).environment(
      SearchEnvironment::selection,
      std::make_shared<const Binding<std::optional<TextSelection>>>(
          std::move(selection)));
}

auto Element::multiline_text_alignment(
    TextAlignmentStrategy strategy) && -> Element {
  return std::move(*this).environment(TextEnvironment::alignment_strategy,
                                      strategy);
}

auto Element::writing_direction(
    WritingDirectionStrategy strategy) && -> Element {
  return std::move(*this).environment(TextEnvironment::writing_direction,
                                      strategy);
}

auto Element::truncation_mode(TruncationMode mode) && -> Element {
  return std::move(*this).environment(TextEnvironment::truncation_mode, mode);
}

auto Element::minimum_scale_factor(float factor) && -> Element {
  return std::move(*this).environment(TextEnvironment::minimum_scale_factor,
                                      std::clamp(factor, 0.01F, 1.0F));
}

auto Element::line_spacing(float spacing) && -> Element {
  return std::move(*this).environment(TextEnvironment::line_spacing, spacing);
}

auto Element::allows_tightening(bool flag) && -> Element {
  return std::move(*this).environment(TextEnvironment::allows_tightening, flag);
}

auto Element::text_case(std::optional<TextCase> text_case) && -> Element {
  return std::move(*this).environment(TextEnvironment::text_case, text_case);
}

auto Element::kerning(float kerning) && -> Element {
  return std::move(*this).environment(TextEnvironment::kerning, kerning);
}

auto Element::tracking(float tracking) && -> Element {
  return std::move(*this).environment(TextEnvironment::tracking, tracking);
}

auto Element::baseline_offset(float offset) && -> Element {
  return std::move(*this).environment(TextEnvironment::baseline_offset, offset);
}

auto Element::monospaced_digit(bool is_active) && -> Element {
  return std::move(*this).environment(TextEnvironment::monospaced_digit,
                                      is_active);
}

auto Element::monospaced(bool is_active) && -> Element {
  return std::move(*this).environment(
      TextEnvironment::font_design,
      is_active ? std::optional{theme::Font::Design::monospaced}
                : std::nullopt);
}

auto Element::italic(bool is_active) && -> Element {
  return std::move(*this).environment(TextEnvironment::italic, is_active);
}

auto Element::bold(bool is_active) && -> Element {
  return std::move(*this).font_weight(
      is_active ? std::optional{theme::Font::Weight::bold} : std::nullopt);
}

auto Element::font_weight(
    std::optional<theme::Font::Weight> weight) && -> Element {
  return std::move(*this).environment(TextEnvironment::font_weight, weight);
}

auto Element::font_design(
    std::optional<theme::Font::Design> design) && -> Element {
  return std::move(*this).environment(TextEnvironment::font_design, design);
}

auto Element::font_width(
    std::optional<theme::Font::Width> width) && -> Element {
  return std::move(*this).environment(TextEnvironment::font_width, width);
}

auto Element::text_scale(TextScale scale, bool is_enabled) && -> Element {
  return std::move(*this).environment(TextEnvironment::text_scale,
                                      is_enabled ? scale : TextScale::standard);
}

auto Element::underline(
    bool is_active, std::optional<theme::ColourValue> colour) && -> Element {
  return std::move(*this).environment(
      TextEnvironment::underline,
      TextDecoration{.active = is_active, .colour = colour});
}

auto Element::strikethrough(
    bool is_active, std::optional<theme::ColourValue> colour) && -> Element {
  return std::move(*this).environment(
      TextEnvironment::strikethrough,
      TextDecoration{.active = is_active, .colour = colour});
}

auto Element::underline(
    bool is_active, text::LinePattern pattern,
    std::optional<theme::ColourValue> colour) && -> Element {
  return std::move(*this).environment(
      TextEnvironment::underline,
      TextDecoration{
          .active = is_active, .colour = colour, .pattern = pattern});
}

auto Element::strikethrough(
    bool is_active, text::LinePattern pattern,
    std::optional<theme::ColourValue> colour) && -> Element {
  return std::move(*this).environment(
      TextEnvironment::strikethrough,
      TextDecoration{
          .active = is_active, .colour = colour, .pattern = pattern});
}

auto Element::redacted(RedactionReasons reason) && -> Element {
  Redaction::Entry entry{.reasons = reason, .resets = false};
  for (const EnvironmentOverride& existing : environment_->overrides) {
    if (existing.key() == Redaction::entry.identity()) {
      const auto& held =
          static_cast<const EnvironmentOverride::Typed<Redaction::Entry>&>(
              existing.value())
              .get();
      entry = Redaction::Entry{.reasons = held.reasons | reason,
                               .resets = held.resets};
    }
  }
  return std::move(*this).environment(Redaction::entry, entry);
}

auto Element::unredacted() && -> Element {
  return std::move(*this).environment(
      Redaction::entry,
      Redaction::Entry{.reasons = RedactionReasons{}, .resets = true});
}

auto Element::privacy_sensitive(bool sensitive) && -> Element {
  return std::move(*this).environment(Redaction::privacy_sensitive, sensitive);
}

auto Element::image_scale(ImageScale scale) && -> Element {
  return std::move(*this).environment(ImageEnvironment::image_scale, scale);
}

auto Element::symbol_variant(SymbolVariants variant) && -> Element {
  return std::move(*this).environment(ImageEnvironment::symbol_variants,
                                      variant);
}

auto Element::symbol_rendering_mode(SymbolRenderingMode mode) && -> Element {
  return std::move(*this).environment(ImageEnvironment::symbol_rendering_mode,
                                      mode);
}

auto Element::symbol_color_rendering_mode(
    std::optional<SymbolColorRenderingMode> mode) && -> Element {
  return std::move(*this).environment(
      ImageEnvironment::symbol_color_rendering_mode, mode);
}

auto Element::symbol_variable_value_mode(
    std::optional<SymbolVariableValueMode> mode) && -> Element {
  return std::move(*this).environment(
      ImageEnvironment::symbol_variable_value_mode, mode);
}

auto Element::symbol_effects_removed(bool is_enabled) && -> Element {
  return std::move(*this).environment(ImageEnvironment::symbol_effects_removed,
                                      is_enabled);
}

auto Element::allowed_dynamic_range(
    std::optional<ImageDynamicRange> range) && -> Element {
  return std::move(*this).environment(ImageEnvironment::allowed_dynamic_range,
                                      range);
}

auto Element::label_icon_to_title_spacing(float value) && -> Element {
  return std::move(*this).environment(
      ControlEnvironment::label_icon_to_title_spacing, value);
}

auto Element::label_reserved_icon_width(float value) && -> Element {
  return std::move(*this).environment(
      ControlEnvironment::label_reserved_icon_width, value);
}

auto Element::slider_thumb_visibility(Visibility visibility) && -> Element {
  return std::move(*this).environment(
      ControlEnvironment::slider_thumb_visibility, visibility);
}

auto Element::horizontal_radio_group_layout() && -> Element {
  return std::move(*this).environment(ControlEnvironment::radio_group_layout,
                                      Axis::horizontal);
}

auto Element::aspect_ratio(std::optional<float> ratio,
                           ContentMode mode) && -> Element {
  if (ratio.has_value() && !(*ratio > 0.0F)) {
    ratio.reset();
  }
  modifiers_.edit().aspect = AspectRatio{.ratio = ratio, .mode = mode};
  return std::move(*this);
}

auto Element::scaled_to_fit() && -> Element {
  return std::move(*this).aspect_ratio(std::nullopt, ContentMode::fit);
}

auto Element::scaled_to_fill() && -> Element {
  return std::move(*this).aspect_ratio(std::nullopt, ContentMode::fill);
}

auto Element::grid_cell() noexcept -> GridCell& {
  if (!modifiers_.edit().grid_cell.has_value()) {
    modifiers_.edit().grid_cell.emplace();
  }
  return *modifiers_.edit().grid_cell;
}

void Element::distribute_decoration() {
  if (content_ == nullptr || !content_->distributes_decoration()) {
    return;
  }
  const Modifiers& own = *modifiers_;
  const bool decorated =
      own.background.has_value() || own.material.has_value() ||
      own.border.has_value() || own.overlay.has_value() ||
      own.drop_shadow.has_value() || own.shadow != theme::Elevation::none;
  if (!decorated || !std::ranges::all_of(children_, [](const Element& child) {
        return child.receives_decoration();
      })) {
    return;
  }
  for (Element& child : children_) {
    child.adopt_decoration(own);
  }
  Modifiers& cleared = modifiers_.edit();
  cleared.background.reset();
  cleared.material.reset();
  cleared.border.reset();
  cleared.overlay.reset();
  cleared.drop_shadow.reset();
  cleared.shadow = theme::Elevation::none;
}

auto Element::receives_decoration() const noexcept -> bool {
  if (content_ == nullptr) {
    return false;
  }
  if (!content_->passes_through()) {
    return true;
  }
  return !content_->composes() &&
         std::ranges::all_of(children_, [](const Element& child) {
           return child.receives_decoration();
         });
}

void Element::adopt_decoration(const Modifiers& row) {
  if (content_->passes_through()) {
    for (Element& child : children_) {
      child.adopt_decoration(row);
    }
    return;
  }
  Modifiers& own = modifiers_.edit();
  if (!own.background.has_value() && !own.material.has_value()) {
    own.background = row.background;
    own.material = row.material;
  }
  if (!own.border.has_value()) {
    own.border = row.border;
  }
  if (!own.overlay.has_value()) {
    own.overlay = row.overlay;
  }
  if (!own.drop_shadow.has_value()) {
    own.drop_shadow = row.drop_shadow;
  }
  if (own.shadow == theme::Elevation::none) {
    own.shadow = row.shadow;
  }
}

auto Element::grid_cell_columns(std::uint32_t count) && -> Element {
  grid_cell().columns = std::max<std::uint32_t>(count, 1);
  return std::move(*this);
}

auto Element::grid_column_alignment(HorizontalAlignment guide) && -> Element {
  grid_cell().column_alignment = guide;
  return std::move(*this);
}

auto Element::grid_cell_anchor(motion::UnitPoint anchor) && -> Element {
  grid_cell().anchor = anchor;
  return std::move(*this);
}

auto Element::grid_cell_unsized_axes(Axes axes) && -> Element {
  grid_cell().unsized_axes = axes;
  return std::move(*this);
}

auto Element::row_attributes() -> ListRow& {
  std::shared_ptr<ListRow>& row = membership_.edit().list_row;
  if (row == nullptr) {
    row = std::make_shared<ListRow>();
  }
  return *row;
}

auto Element::list_row() const noexcept -> std::shared_ptr<const ListRow> {
  return membership_->list_row;
}

auto Element::list_row_insets(
    std::optional<theme::Insets> insets) && -> Element {
  row_attributes().insets = insets;
  return std::move(*this);
}

auto Element::list_row_insets(theme::Edge edges,
                              std::optional<float> length) && -> Element {
  row_attributes().edge_insets.set(edges, length);
  return std::move(*this);
}

auto Element::list_row_separator(Visibility visibility,
                                 VerticalEdges edges) && -> Element {
  ListRow& row = row_attributes();
  if (edges.contains(VerticalEdges::top)) {
    row.top.visibility = visibility;
  }
  if (edges.contains(VerticalEdges::bottom)) {
    row.bottom.visibility = visibility;
  }
  return std::move(*this);
}

auto Element::list_row_separator_tint(std::optional<theme::ColourValue> colour,
                                      VerticalEdges edges) && -> Element {
  ListRow& row = row_attributes();
  if (edges.contains(VerticalEdges::top)) {
    row.top.tint = colour;
  }
  if (edges.contains(VerticalEdges::bottom)) {
    row.bottom.tint = colour;
  }
  return std::move(*this);
}

auto Element::list_row_background(Element view) && -> Element {
  row_attributes().background =
      std::make_shared<PlaceholderContentView::Source>(std::move(view));
  return std::move(*this);
}

auto Element::list_item_tint(
    std::optional<theme::Colour> colour) && -> Element {
  row_attributes().item_tint =
      colour.has_value()
          ? std::optional<ListItemTint>{ListItemTint::fixed(*colour)}
          : std::nullopt;
  return std::move(*this);
}

auto Element::list_item_tint(ListItemTint tint) && -> Element {
  row_attributes().item_tint = tint;
  return std::move(*this);
}

auto Element::labels_hidden() && -> Element {
  return std::move(*this).labels_visibility(Visibility::hidden);
}

auto Element::labels_visibility(Visibility visibility) && -> Element {
  return std::move(*this).environment(ControlEnvironment::labels_visibility,
                                      visibility);
}

auto Element::menu_indicator(Visibility visibility) && -> Element {
  return std::move(*this).environment(ControlEnvironment::menu_indicator,
                                      visibility);
}

auto Element::menu_order(MenuOrder order) && -> Element {
  return std::move(*this).environment(ControlEnvironment::menu_order, order);
}

auto Element::menu_action_dismiss_behavior(
    MenuActionDismissBehavior behavior) && -> Element {
  return std::move(*this).environment(
      ControlEnvironment::menu_action_dismiss_behavior, behavior);
}

auto Element::badge(int count) && -> Element {
  row_attributes().badge =
      count == 0 ? std::nullopt
                 : std::optional<std::string>{std::format("{}", count)};
  return std::move(*this);
}

auto Element::badge(std::optional<std::string> label) && -> Element {
  row_attributes().badge = std::move(label);
  return std::move(*this);
}

auto Element::selection_disabled(bool disabled) && -> Element {
  row_attributes().selection_disabled = disabled;
  return std::move(*this);
}

auto Element::move_disabled(bool disabled) && -> Element {
  row_attributes().move_disabled = disabled;
  return std::move(*this);
}

auto Element::delete_disabled(bool disabled) && -> Element {
  row_attributes().delete_disabled = disabled;
  return std::move(*this);
}

auto Element::swipe_actions(std::function<Element()> content) && -> Element {
  return std::move(*this).swipe_actions(HorizontalEdge::trailing, true,
                                        std::move(content));
}

auto Element::swipe_actions(HorizontalEdge edge, bool allows_full_swipe,
                            std::function<Element()> content) && -> Element {
  ListRow& row = row_attributes();
  ListRow::SwipeActions& actions =
      edge == HorizontalEdge::leading ? row.leading : row.trailing;
  actions.content = std::move(content);
  actions.allows_full_swipe = allows_full_swipe;
  return std::move(*this);
}

auto Element::list_row_hover_effect(
    std::optional<motion::HoverEffect> effect) && -> Element {
  row_attributes().hover = effect;
  return std::move(*this);
}

auto Element::list_row_hover_effect_disabled(bool disabled) && -> Element {
  row_attributes().hover_disabled = disabled;
  return std::move(*this);
}

auto Element::list_row_spacing(std::optional<float> spacing) && -> Element {
  return std::move(*this).environment(ListEnvironment::row_spacing, spacing);
}

auto Element::list_section_spacing(std::optional<float> spacing) && -> Element {
  row_attributes().section_spacing = spacing;
  return std::move(*this).environment(ListEnvironment::section_spacing,
                                      spacing);
}

auto Element::default_min_list_row_height(
    std::optional<float> height) && -> Element {
  return std::move(*this).environment(ListEnvironment::default_min_row_height,
                                      height);
}

auto Element::alternating_row_backgrounds(bool enabled) && -> Element {
  return std::move(*this).environment(
      ListEnvironment::alternating_row_backgrounds, enabled);
}

auto Element::alternating_row_backgrounds(
    AlternatingRowBackgroundBehavior behavior) && -> Element {
  return std::move(*this).environment(
      ListEnvironment::alternating_row_background_behavior, behavior);
}

auto Element::header_prominence(Prominence prominence) && -> Element {
  return std::move(*this).environment(ListEnvironment::header_prominence,
                                      prominence);
}

auto Element::list_section_spacing(ListSectionSpacing spacing) && -> Element {
  return std::move(*this).list_section_spacing(spacing.length());
}

auto Element::list_section_margins(theme::Edge edges,
                                   std::optional<float> length) && -> Element {
  return std::move(*this).transform_environment(
      ListEnvironment::section_margins,
      [edges, length](EdgeLengths& margins) { margins.set(edges, length); });
}

auto Element::list_section_separator(Visibility visibility,
                                     VerticalEdges edges) && -> Element {
  Element result = std::move(*this);
  if (edges.contains(VerticalEdges::top)) {
    result = std::move(result).transform_environment(
        ListEnvironment::section_separator_top,
        [visibility](ListRow::Separator& separator) {
          separator.visibility = visibility;
        });
  }
  if (edges.contains(VerticalEdges::bottom)) {
    result = std::move(result).transform_environment(
        ListEnvironment::section_separator_bottom,
        [visibility](ListRow::Separator& separator) {
          separator.visibility = visibility;
        });
  }
  return result;
}

auto Element::list_section_separator_tint(
    std::optional<theme::ColourValue> colour,
    VerticalEdges edges) && -> Element {
  Element result = std::move(*this);
  if (edges.contains(VerticalEdges::top)) {
    result = std::move(result).transform_environment(
        ListEnvironment::section_separator_top,
        [colour](ListRow::Separator& separator) { separator.tint = colour; });
  }
  if (edges.contains(VerticalEdges::bottom)) {
    result = std::move(result).transform_environment(
        ListEnvironment::section_separator_bottom,
        [colour](ListRow::Separator& separator) { separator.tint = colour; });
  }
  return result;
}

auto Element::default_min_list_header_height(
    std::optional<float> height) && -> Element {
  return std::move(*this).environment(
      ListEnvironment::default_min_header_height, height);
}

auto Element::badge_prominence(BadgeProminence prominence) && -> Element {
  row_attributes().badge_prominence = prominence;
  return std::move(*this).environment(ListEnvironment::badge_prominence,
                                      prominence);
}

auto Element::section_actions(std::function<Element()> content) && -> Element {
  auto shared =
      std::make_shared<const std::function<Element()>>(std::move(content));
  row_attributes().section_actions = [shared] { return (*shared)(); };
  return std::move(*this).environment(ListEnvironment::section_actions,
                                      ListEnvironment::SectionActions{shared});
}

auto Element::table_column_headers(Visibility visibility) && -> Element {
  return std::move(*this).environment(ListEnvironment::table_column_headers,
                                      visibility);
}

auto Element::has_swipe_actions() const noexcept -> bool {
  const std::shared_ptr<ListRow>& row = membership_->list_row;
  return row != nullptr && (static_cast<bool>(row->leading.content) ||
                            static_cast<bool>(row->trailing.content));
}

auto Element::list_section_index_visibility(
    Visibility visibility) && -> Element {
  return std::move(*this).environment(ListEnvironment::section_index_visibility,
                                      visibility);
}

auto Element::section_index_label(
    std::optional<std::string> label) && -> Element {
  row_attributes().index_label = std::move(label);
  return std::move(*this);
}

auto Element::on_move_command(Command::MoveAction action) && -> Element {
  interactions_.edit().commands.push_back(
      std::make_shared<Command>(Command::move(std::move(action))));
  return std::move(*this);
}

auto Element::on_focus_edge(MoveCommandDirection direction,
                            Command::Action action) && -> Element {
  interactions_.edit().commands.push_back(
      std::make_shared<Command>(Command::edge(direction, std::move(action))));
  return std::move(*this);
}

auto Element::on_move_command(Command::MoveAction action,
                              Axis axis) && -> Element {
  interactions_.edit().commands.push_back(
      std::make_shared<Command>(Command::move(std::move(action), axis)));
  return std::move(*this);
}

auto Element::on_exit_command(Command::Action action) && -> Element {
  return std::move(*this).on_exit_command(std::string{Command::exit_label},
                                          std::move(action));
}

auto Element::on_exit_command(std::string label,
                              Command::Action action) && -> Element {
  return std::move(*this).on_exit_command(std::move(label), std::move(action),
                                          CommandOptions{});
}

auto Element::on_exit_command(std::string label, Command::Action action,
                              CommandOptions options) && -> Element {
  interactions_.edit().commands.push_back(std::make_shared<Command>(
      Command::exit(std::move(label), std::move(action), options)));
  return std::move(*this);
}

auto Element::on_play_pause_command(Command::Action action) && -> Element {
  return std::move(*this).on_play_pause_command(
      std::string{Command::play_pause_label}, std::move(action));
}

auto Element::on_play_pause_command(std::string label,
                                    Command::Action action) && -> Element {
  interactions_.edit().commands.push_back(std::make_shared<Command>(
      Command::play_pause(std::move(label), std::move(action))));
  return std::move(*this);
}

auto Element::on_command(provider::Button button, std::string label,
                         Command::Action action,
                         CommandOptions options) && -> Element {
  interactions_.edit().commands.push_back(std::make_shared<Command>(
      Command::button(button, std::move(label), std::move(action), options)));
  return std::move(*this);
}

auto Element::on_key_press(
    KeyEquivalent key,
    std::move_only_function<KeyPress::Result()> action) && -> Element {
  return std::move(*this).on_key_press(
      key, KeyPress::Phases::down | KeyPress::Phases::repeat,
      [action = std::move(action)](const KeyPress&) mutable {
        return action();
      });
}

auto Element::on_key_press(KeyEquivalent key, KeyPress::Phases phases,
                           KeyPressHandler::Action action) && -> Element {
  std::vector<KeyEquivalent> keys;
  keys.push_back(key);
  return std::move(*this).on_key_press(std::move(keys), phases,
                                       std::move(action));
}

auto Element::on_key_press(std::vector<KeyEquivalent> keys,
                           KeyPress::Phases phases,
                           KeyPressHandler::Action action) && -> Element {
  interactions_.edit().key_presses.push_back(std::make_shared<KeyPressHandler>(
      std::move(keys), std::string{}, phases, std::move(action)));
  return std::move(*this);
}

auto Element::on_key_press(KeyPress::Phases phases,
                           KeyPressHandler::Action action) && -> Element {
  return std::move(*this).on_key_press(std::vector<KeyEquivalent>{}, phases,
                                       std::move(action));
}

auto Element::on_key_press(std::string characters, KeyPress::Phases phases,
                           KeyPressHandler::Action action) && -> Element {
  interactions_.edit().key_presses.push_back(std::make_shared<KeyPressHandler>(
      std::vector<KeyEquivalent>{}, std::move(characters), phases,
      std::move(action)));
  return std::move(*this);
}

auto Element::keyboard_shortcut(KeyEquivalent key,
                                EventModifiers modifiers) && -> Element {
  return std::move(*this).keyboard_shortcut(
      KeyboardShortcut{.key = key, .modifiers = modifiers});
}

auto Element::keyboard_shortcut(
    std::optional<KeyboardShortcut> shortcut) && -> Element {
  interactions_.edit().keyboard_shortcut = shortcut;
  return std::move(*this).environment(KeyboardEnvironment::shortcut, shortcut);
}

auto Element::keyboard_shortcut(
    KeyEquivalent key, EventModifiers modifiers,
    KeyboardShortcut::Localization localization) && -> Element {
  return std::move(*this).keyboard_shortcut(KeyboardShortcut{
      .key = key, .modifiers = modifiers, .localization = localization});
}

auto Element::on_modifier_keys_changed(
    EventModifiers mask, bool initial,
    ModifierKeysHandler::Action action) && -> Element {
  interactions_.edit().modifier_keys.push_back(
      std::make_shared<ModifierKeysHandler>(mask, initial, std::move(action)));
  return std::move(*this);
}

auto Element::on_modifier_keys_changed(
    ModifierKeysHandler::Action action) && -> Element {
  return std::move(*this).on_modifier_keys_changed(EventModifiers::all, true,
                                                   std::move(action));
}

auto Element::modifier_key_alternate(EventModifiers modifiers,
                                     Element alternate) && -> Element {
  return Element{
      ModifierKeyAlternate(modifiers, std::move(*this), std::move(alternate))};
}

auto Element::type_select_equivalent(
    std::optional<std::string> text) && -> Element {
  interactions_.edit().type_select_cleared = !text.has_value();
  interactions_.edit().type_select = std::move(text);
  return std::move(*this);
}

auto Element::on_delete_command(Command::Action action) && -> Element {
  auto shared = std::make_shared<Command>(
      Command::remove(std::string{Command::remove_label}, std::move(action)));
  interactions_.edit().commands.push_back(shared);
  std::vector<KeyEquivalent> keys;
  keys.push_back(KeyEquivalent::delete_key);
  keys.push_back(KeyEquivalent::delete_forward);
  interactions_.edit().key_presses.push_back(std::make_shared<KeyPressHandler>(
      std::move(keys), std::string{}, KeyPress::Phases::down,
      [shared](const KeyPress&) {
        shared->perform();
        return KeyPress::Result::handled;
      }));
  return std::move(*this);
}

auto Element::page_command(Binding<int> value, int lower, int upper,
                           int step) && -> Element {
  const int low = std::min(lower, upper);
  const int high = std::max(lower, upper);
  const int stride = std::max(step, 1);
  const auto turn = [value, low, high, stride](bool forward) {
    const int next =
        std::clamp(value.get() + (forward ? stride : -stride), low, high);
    if (next != value.get()) {
      value.set(next);
    }
  };
  const CommandOptions hidden{.trigger = CommandTrigger::repeat,
                              .hidden = true};
  interactions_.edit().commands.push_back(
      std::make_shared<Command>(Command::page(
          false, std::string{Command::previous_label}, [turn] { turn(false); },
          hidden)));
  interactions_.edit().commands.push_back(
      std::make_shared<Command>(Command::page(
          true, std::string{Command::next_label}, [turn] { turn(true); },
          hidden)));
  std::vector<KeyEquivalent> keys;
  keys.push_back(KeyEquivalent::page_up);
  keys.push_back(KeyEquivalent::page_down);
  interactions_.edit().key_presses.push_back(std::make_shared<KeyPressHandler>(
      std::move(keys), std::string{},
      KeyPress::Phases::down | KeyPress::Phases::repeat,
      [turn](const KeyPress& press) {
        turn(press.key == KeyEquivalent::page_down);
        return KeyPress::Result::handled;
      }));
  return std::move(*this);
}

auto Element::transaction(TransactionEdit edit) && -> Element {
  TransactionEdit& held = lifecycle_.edit().transaction;
  if (!held) {
    held = std::move(edit);
    return std::move(*this);
  }
  held = [first = std::move(held),
          second = std::move(edit)](motion::Transaction& transaction) mutable {
    first(transaction);
    second(transaction);
  };
  return std::move(*this);
}

auto Element::navigation_transition(
    NavigationTransition transition) && -> Element {
  navigation_.edit().transition = std::move(transition);
  return std::move(*this);
}

auto Element::matched_transition_source(ElementKey id,
                                        Namespace::ID space) && -> Element {
  return std::move(*this).matched_geometry_effect(
      std::move(id), space,
      motion::MatchedGeometryProperties::frame |
          motion::MatchedGeometryProperties::corner_radius);
}

auto Element::matched_transition_source(
    ElementKey id, Namespace::ID space,
    const std::function<MatchedTransitionSourceConfiguration(
        MatchedTransitionSourceConfiguration)>& configuration) && -> Element {
  const MatchedTransitionSourceConfiguration styled =
      configuration ? configuration(MatchedTransitionSourceConfiguration{})
                    : MatchedTransitionSourceConfiguration{};
  Element source = std::move(*this);
  const theme::Shape outline = styled.clip().value_or(theme::Rectangle());
  if (styled.background_style().has_value()) {
    source = std::move(source).background(*styled.background_style(), outline);
  }
  if (styled.clip().has_value()) {
    source = std::move(source).clip_shape(*styled.clip());
  }
  if (const auto& cast = styled.cast(); cast.has_value()) {
    source =
        std::move(source).shadow(cast->colour, cast->radius, cast->x, cast->y);
  }
  return std::move(source).matched_transition_source(std::move(id), space);
}

auto Element::geometry_group() && -> Element {
  modifiers_.edit().geometry_group = true;
  return std::move(*this);
}

auto Element::symbol_effect(SymbolEffect effect, bool is_active) && -> Element {
  std::optional<ElementKey> key = std::exchange(key_, std::nullopt);
  auto source =
      std::make_shared<PlaceholderContentView::Source>(std::move(*this));
  if (effect.runs_on_presentation()) {
    return Element{SymbolEffectAnimator<bool>{
        effect, is_active, std::move(source), std::move(key)}};
  }
  Element effected{SymbolEffectAnimator<bool>{
      effect, is_active, SymbolEffect::Playback::endless, std::move(source)}};
  if (key.has_value()) {
    return std::move(effected).id(std::move(*key));
  }
  return effected;
}

auto Element::on_tap_gesture(
    std::move_only_function<void()> action) && -> Element {
  return std::move(*this).on_tap_gesture(1, std::move(action));
}

auto Element::on_tap_gesture(
    int count, std::move_only_function<void()> action) && -> Element {
  return std::move(*this).gesture(
      TapGesture(count).on_ended(std::move(action)));
}

auto Element::on_long_press_gesture(
    std::move_only_function<void()> perform) && -> Element {
  return std::move(*this).on_long_press_gesture(
      LongPressGesture::default_duration, LongPressGesture::default_distance,
      std::move(perform), {});
}

auto Element::on_long_press_gesture(
    double minimum_duration, std::move_only_function<void()> perform,
    LongPressGesture::Pressing on_pressing_changed) && -> Element {
  return std::move(*this).on_long_press_gesture(
      minimum_duration, LongPressGesture::default_distance, std::move(perform),
      std::move(on_pressing_changed));
}

auto Element::on_long_press_gesture(
    double minimum_duration, float maximum_distance,
    std::move_only_function<void()> perform,
    LongPressGesture::Pressing on_pressing_changed) && -> Element {
  LongPressGesture pressed =
      LongPressGesture(minimum_duration, maximum_distance)
          .on_ended([perform = std::move(perform)](const bool&) mutable {
            perform();
          });
  if (on_pressing_changed) {
    pressed =
        std::move(pressed).on_pressing_changed(std::move(on_pressing_changed));
  }
  return std::move(*this).gesture(std::move(pressed));
}

auto Element::attach(Gesture gesture, GesturePriority priority,
                     GestureMask mask, std::string name) && -> Element {
  auto shared = std::make_shared<Gesture>(std::move(gesture));
  interactions_.edit().gestures.push_back(
      GestureAttachment{.gesture = shared,
                        .priority = priority,
                        .mask = mask,
                        .handoff = std::nullopt,
                        .name = std::move(name)});
  if ((mask & GestureMask::gesture) == GestureMask::none) {
    return std::move(*this);
  }
  bool taps = false;
  bool holds = false;
  const GestureLayout& layout = shared->layout();
  for (std::size_t index = 0; index < layout.size(); ++index) {
    std::visit(
        [&taps, &holds](const auto& leaf) {
          using Leaf = std::remove_cvref_t<decltype(leaf)>;
          if (!leaf.input_kinds().contains(GestureInputKinds::controller)) {
            return;
          }
          if constexpr (std::same_as<Leaf, TapGesture> ||
                        std::same_as<Leaf, SpatialTapGesture>) {
            taps = true;
          } else if constexpr (std::same_as<Leaf, LongPressGesture>) {
            holds = true;
          }
        },
        layout.leaf(index));
  }
  if (!taps && !holds) {
    return std::move(*this);
  }
  if (!interactions_.edit().focusable.has_value()) {
    interactions_.edit().focusable = true;
  }
  auto size = std::make_shared<Size>();
  Element controlled = std::move(*this).on_geometry_change(
      std::type_identity<Size>{},
      [](const GeometryProxy& proxy) { return proxy.size(); },
      [size](Size value) { *size = value; });
  if (taps) {
    controlled =
        std::move(controlled)
            .on_command(provider::Button::a,
                        std::string{Gesture::controller_label},
                        [shared, size] { shared->controller_tap(*size); });
  }
  if (holds) {
    controlled =
        std::move(controlled)
            .on_command(
                provider::Button::a, std::string{Gesture::controller_label},
                [shared] { shared->controller_hold(); },
                CommandOptions{.trigger = CommandTrigger::long_press,
                               .hidden = true});
  }
  return controlled;
}

auto Element::gesture(Gesture gesture) && -> Element {
  return std::move(*this).attach(std::move(gesture), GesturePriority::normal,
                                 GestureMask::all, {});
}

auto Element::simultaneous_gesture(Gesture gesture) && -> Element {
  return std::move(*this).attach(
      std::move(gesture), GesturePriority::simultaneous, GestureMask::all, {});
}

auto Element::high_priority_gesture(Gesture gesture) && -> Element {
  return std::move(*this).attach(std::move(gesture), GesturePriority::high,
                                 GestureMask::all, {});
}

auto Element::gesture(Gesture gesture, GestureMask mask) && -> Element {
  return std::move(*this).attach(std::move(gesture), GesturePriority::normal,
                                 mask, {});
}

auto Element::gesture(Gesture gesture, bool is_enabled) && -> Element {
  return std::move(*this).gesture(
      std::move(gesture),
      is_enabled ? GestureMask::all : GestureMask::subviews);
}

auto Element::simultaneous_gesture(Gesture gesture,
                                   GestureMask mask) && -> Element {
  return std::move(*this).attach(std::move(gesture),
                                 GesturePriority::simultaneous, mask, {});
}

auto Element::simultaneous_gesture(Gesture gesture,
                                   bool is_enabled) && -> Element {
  return std::move(*this).simultaneous_gesture(
      std::move(gesture),
      is_enabled ? GestureMask::all : GestureMask::subviews);
}

auto Element::high_priority_gesture(Gesture gesture,
                                    GestureMask mask) && -> Element {
  return std::move(*this).attach(std::move(gesture), GesturePriority::high,
                                 mask, {});
}

auto Element::high_priority_gesture(Gesture gesture,
                                    bool is_enabled) && -> Element {
  return std::move(*this).high_priority_gesture(
      std::move(gesture),
      is_enabled ? GestureMask::all : GestureMask::subviews);
}

auto Element::named_gesture(Gesture gesture, GesturePriority priority,
                            std::string_view name,
                            bool is_enabled) && -> Element {
  return std::move(*this).attach(
      std::move(gesture), priority,
      is_enabled ? GestureMask::all : GestureMask::subviews, std::string{name});
}

auto Element::attach_drag(
    std::vector<TransferItem> items, std::optional<ElementKey> id,
    std::optional<Namespace::ID> container) && -> Element {
  interactions_.edit().drag = std::make_shared<const DragSource>(
      std::move(items), std::move(id), container);
  return std::move(*this);
}

auto Element::on_drop(std::vector<UTType> types,
                      std::shared_ptr<DropDelegate> delegate) && -> Element {
  interactions_.edit().drop =
      std::make_shared<DropTarget>(std::move(types), std::move(delegate));
  return std::move(*this);
}

auto Element::on_drop(std::vector<UTType> types,
                      std::optional<Binding<bool>> is_targeted,
                      std::move_only_function<bool(std::vector<TransferItem>)>
                          perform) && -> Element {
  return std::move(*this).on_drop(
      std::move(types), std::move(is_targeted),
      std::move_only_function<bool(std::vector<TransferItem>, Point)>{
          [perform = std::move(perform)](std::vector<TransferItem> items,
                                         Point) mutable {
            return perform(std::move(items));
          }});
}

auto Element::on_drop(
    std::vector<UTType> types, std::optional<Binding<bool>> is_targeted,
    std::move_only_function<bool(std::vector<TransferItem>, Point)>
        perform) && -> Element {
  DropTarget::Targeted targeted;
  if (is_targeted.has_value()) {
    targeted = [binding = std::move(*is_targeted)](bool inside) {
      binding.set(inside);
    };
  }
  interactions_.edit().drop = std::make_shared<DropTarget>(
      std::move(types),
      DropTarget::Perform{[perform = std::move(perform)](
                              std::span<const TransferItem> items,
                              Point location) mutable {
        return perform(std::vector<TransferItem>(items.begin(), items.end()),
                       location);
      }},
      std::move(targeted));
  return std::move(*this);
}

auto Element::on_drag(const std::function<TransferItem()>& data) && -> Element {
  if (!data) {
    return std::move(*this);
  }
  std::vector<TransferItem> items;
  items.push_back(data());
  return std::move(*this).attach_drag(std::move(items), std::nullopt,
                                      std::nullopt);
}

auto Element::drag_configuration(
    DragConfiguration configuration) && -> Element {
  interactions_.edit().drag_configuration = configuration;
  return std::move(*this);
}

auto Element::drop_configuration(
    std::move_only_function<DropConfiguration(const DropSession&)>
        configuration) && -> Element {
  interactions_.edit().drop_configuration = std::make_shared<
      std::move_only_function<DropConfiguration(const DropSession&)>>(
      std::move(configuration));
  return std::move(*this);
}

auto Element::on_drag_session_updated(
    std::move_only_function<void(const DragSession&)> action) && -> Element {
  interactions_.edit().drag_updates =
      std::make_shared<std::move_only_function<void(const DragSession&)>>(
          std::move(action));
  return std::move(*this);
}

auto Element::on_drop_session_updated(
    std::move_only_function<void(const DropSession&)> action) && -> Element {
  interactions_.edit().drop_updates =
      std::make_shared<std::move_only_function<void(const DropSession&)>>(
          std::move(action));
  return std::move(*this);
}

auto Element::drag_previews_formation(
    DragDropPreviewsFormation formation) && -> Element {
  interactions_.edit().drag_previews = formation;
  return std::move(*this);
}

auto Element::drop_previews_formation(
    DragDropPreviewsFormation formation) && -> Element {
  interactions_.edit().drop_previews = formation;
  return std::move(*this);
}

auto Element::spring_loading_behavior(
    SpringLoadingBehavior behavior) && -> Element {
  interactions_.edit().spring_loading = behavior;
  return std::move(*this).environment(SpringLoading::behavior, behavior);
}

auto Element::draggable_container_item(
    ElementKey id,
    std::optional<Namespace::ID> container_namespace) && -> Element {
  return std::move(*this).attach_drag({}, std::move(id), container_namespace);
}

auto Element::reorder_item(ElementKey id,
                           std::optional<ElementKey> collection) && -> Element {
  std::vector<TransferItem> items;
  items.emplace_back(ReorderItem::content_type(), id.description());
  interactions_.edit().reorder_item =
      ReorderItem{.id = id, .collection = std::move(collection)};
  return std::move(*this).attach_drag(std::move(items), std::move(id),
                                      std::nullopt);
}

auto Element::on_tap_gesture(
    int count, CoordinateSpace coordinate_space, GestureInputKinds input_kinds,
    std::move_only_function<void(Point)> action) && -> Element {
  return std::move(*this).gesture(
      SpatialTapGesture(count, std::move(coordinate_space), input_kinds)
          .on_ended([action = std::move(action)](
                        const SpatialTapGesture::Value& value) mutable {
            action(value.location);
          }));
}

auto Element::on_long_press_gesture(
    double minimum_duration, float maximum_distance,
    GestureInputKinds input_kinds, std::move_only_function<void()> perform,
    LongPressGesture::Pressing on_pressing_changed) && -> Element {
  LongPressGesture pressed =
      LongPressGesture(minimum_duration, maximum_distance, input_kinds)
          .on_ended([perform = std::move(perform)](const bool&) mutable {
            perform();
          });
  if (on_pressing_changed) {
    pressed =
        std::move(pressed).on_pressing_changed(std::move(on_pressing_changed));
  }
  return std::move(*this).gesture(std::move(pressed));
}

auto Element::on_tap_gesture(
    int count, CoordinateSpace coordinate_space,
    std::move_only_function<void(Point)> action) && -> Element {
  return std::move(*this).gesture(
      SpatialTapGesture(count, std::move(coordinate_space))
          .on_ended([action = std::move(action)](
                        const SpatialTapGesture::Value& value) mutable {
            action(value.location);
          }));
}

auto Element::content_shape(theme::Shape shape) && -> Element {
  return std::move(*this).content_shape(ContentShapeKinds::interaction, shape);
}

auto Element::content_shape(ContentShapeKinds kinds,
                            theme::Shape shape) && -> Element {
  const bool exact = shape.form == theme::ShapeForm::ellipse ||
                     shape.form == theme::ShapeForm::uneven ||
                     shape.form == theme::ShapeForm::circle ||
                     shape.elliptical();
  if (kinds.contains(ContentShapeKinds::accessibility)) {
    interactions_.edit().accessibility_shape = graphics::AnyShape{shape};
  }
  if (kinds.contains(ContentShapeKinds::context_menu_preview)) {
    interactions_.edit().context_menu_preview_shape = graphics::AnyShape{shape};
  }
  if (kinds.contains(ContentShapeKinds::interaction)) {
    interactions_.edit().hit_shape = shape;
    if (exact) {
      interactions_.edit().hit_path = graphics::AnyShape{shape};
      interactions_.edit().hit_fill = graphics::FillStyle{};
    } else {
      interactions_.edit().hit_path.reset();
    }
  }
  if (kinds.contains(ContentShapeKinds::focus_effect)) {
    interactions_.edit().focus_shape = shape;
  }
  if (kinds.contains(ContentShapeKinds::hover_effect)) {
    interactions_.edit().hover_shape = shape;
  }
  if (kinds.contains(ContentShapeKinds::drag_preview)) {
    interactions_.edit().drag_preview_shape = shape;
  }
  return std::move(*this);
}

auto Element::content_shape(graphics::AnyShape shape,
                            bool eo_fill) && -> Element {
  return std::move(*this).content_shape(ContentShapeKinds::interaction,
                                        std::move(shape), eo_fill);
}

auto Element::content_shape(ContentShapeKinds kinds, graphics::AnyShape shape,
                            bool eo_fill) && -> Element {
  if (const std::optional<theme::Shape> leaf = shape.leaf();
      leaf.has_value() && graphics::AnyShape{*leaf} == shape && !eo_fill) {
    return std::move(*this).content_shape(kinds, *leaf);
  }
  if (kinds.contains(ContentShapeKinds::accessibility)) {
    interactions_.edit().accessibility_shape = shape;
  }
  if (kinds.contains(ContentShapeKinds::context_menu_preview)) {
    interactions_.edit().context_menu_preview_shape = shape;
  }
  if (kinds.contains(ContentShapeKinds::interaction)) {
    interactions_.edit().hit_shape = theme::Shape::rectangle();
    interactions_.edit().hit_path = shape;
    interactions_.edit().hit_fill = graphics::FillStyle{.eo_fill = eo_fill};
  }
  if (kinds.contains(ContentShapeKinds::focus_effect)) {
    interactions_.edit().focus_shape = theme::Shape::rectangle();
  }
  if (kinds.contains(ContentShapeKinds::hover_effect)) {
    interactions_.edit().hover_shape = theme::Shape::rectangle();
  }
  if (kinds.contains(ContentShapeKinds::drag_preview)) {
    interactions_.edit().drag_preview_shape = theme::Shape::rectangle();
  }
  return std::move(*this);
}

auto Element::scroll_disabled(bool disabled) && -> Element {
  interactions_.edit().scrolling.disabled = disabled;
  return std::move(*this).transform_environment(
      ScrollEnvironment::is_scroll_enabled,
      [disabled](bool& enabled) { enabled = enabled && !disabled; });
}

auto Element::scroll_target_behavior(
    ScrollTargetBehavior behavior) && -> Element {
  interactions_.edit().scrolling.target_behavior = behavior;
  interactions_.edit().scrolling.aligned_behavior.reset();
  return std::move(*this);
}

auto Element::scroll_target_behavior(
    AnyScrollTargetBehavior behavior) && -> Element {
  interactions_.edit().scrolling.custom_target_behavior = std::move(behavior);
  return std::move(*this);
}

auto Element::scroll_target_behavior(
    PagingScrollTargetBehavior behavior) && -> Element {
  static_cast<void>(behavior);
  interactions_.edit().scrolling.target_behavior = ScrollTargetBehavior::paging;
  interactions_.edit().scrolling.aligned_behavior.reset();
  return std::move(*this);
}

auto Element::scroll_target_behavior(
    ViewAlignedScrollTargetBehavior behavior) && -> Element {
  interactions_.edit().scrolling.target_behavior =
      ScrollTargetBehavior::view_aligned;
  interactions_.edit().scrolling.aligned_behavior = behavior;
  return std::move(*this);
}

auto Element::scroll_indicators(ScrollIndicatorVisibility visibility,
                                Axes axes) && -> Element {
  interactions_.edit().scrolling.indicators.set(axes, visibility);
  Element shown = std::move(*this);
  if (axes.contains(Axis::horizontal)) {
    shown = std::move(shown).environment(
        ScrollEnvironment::horizontal_scroll_indicator_visibility, visibility);
  }
  if (axes.contains(Axis::vertical)) {
    shown = std::move(shown).environment(
        ScrollEnvironment::vertical_scroll_indicator_visibility, visibility);
  }
  return shown;
}

auto Element::scroll_indicators_flash(bool on_appear) && -> Element {
  interactions_.edit().scrolling.flash =
      Scrolling::Flash{.trigger = std::nullopt, .on_appear = on_appear};
  return std::move(*this);
}

auto Element::scroll_bounce_behavior(ScrollBounceBehavior behavior,
                                     Axes axes) && -> Element {
  interactions_.edit().scrolling.bounce.set(axes, behavior);
  Element bounced = std::move(*this);
  if (axes.contains(Axis::horizontal)) {
    bounced = std::move(bounced).environment(
        ScrollEnvironment::horizontal_scroll_bounce_behavior, behavior);
  }
  if (axes.contains(Axis::vertical)) {
    bounced = std::move(bounced).environment(
        ScrollEnvironment::vertical_scroll_bounce_behavior, behavior);
  }
  return bounced;
}

auto Element::scroll_deceleration_rate(
    ScrollDecelerationRate rate) && -> Element {
  interactions_.edit().scrolling.deceleration = rate;
  return std::move(*this);
}

auto Element::scroll_dismisses_keyboard(
    ScrollDismissesKeyboardMode mode) && -> Element {
  interactions_.edit().scrolling.dismisses_keyboard = mode;
  return std::move(*this).environment(
      ScrollEnvironment::scroll_dismisses_keyboard_mode, mode);
}

auto Element::scroll_input_behavior(ScrollInputBehavior behavior,
                                    ScrollInputKind input) && -> Element {
  interactions_.edit().scrolling.inputs.set(input, behavior);
  return std::move(*this);
}

auto Element::default_scroll_anchor(
    std::optional<motion::UnitPoint> anchor) && -> Element {
  interactions_.edit().scrolling.anchors = Scrolling::Anchors{
      .initial_offset = anchor, .size_changes = anchor, .alignment = anchor};
  return std::move(*this);
}

auto Element::default_scroll_anchor(std::optional<motion::UnitPoint> anchor,
                                    ScrollAnchorRole role) && -> Element {
  switch (role) {
    case ScrollAnchorRole::initial_offset:
      interactions_.edit().scrolling.anchors.initial_offset = anchor;
      break;
    case ScrollAnchorRole::size_changes:
      interactions_.edit().scrolling.anchors.size_changes = anchor;
      break;
    case ScrollAnchorRole::alignment:
      interactions_.edit().scrolling.anchors.alignment = anchor;
      break;
  }
  return std::move(*this);
}

auto Element::scroll_position(
    Binding<ScrollPosition> position,
    std::optional<motion::UnitPoint> anchor) && -> Element {
  interactions_.edit().scrolling.scroll_position =
      std::make_shared<const Binding<ScrollPosition>>(std::move(position));
  interactions_.edit().scrolling.scroll_position_anchor = anchor;
  return std::move(*this);
}

auto Element::scroll_edge_effect_style(
    std::optional<ScrollEdgeEffectStyle> style,
    theme::Edge edges) && -> Element {
  if (style.has_value()) {
    interactions_.edit().scrolling.edge_effects.style(*style, edges);
    interactions_.edit().scrolling.edge_effects.hide(false, edges);
  } else {
    interactions_.edit().scrolling.edge_effects.hide(true, edges);
  }
  for (const ToolbarPlacement bars :
       {ToolbarPlacement::navigation_bar, ToolbarPlacement::bottom_bar}) {
    const theme::Edge edge = bars == ToolbarPlacement::navigation_bar
                                 ? theme::Edge::top
                                 : theme::Edge::bottom;
    if ((std::to_underlying(edges) & std::to_underlying(edge)) == 0U) {
      continue;
    }
    NavigationChrome::Bar& bar = navigation_.edit().chrome.edit_bar(bars);
    bar.scroll_edge_effect_hidden = !style.has_value();
    if (style.has_value()) {
      bar.scroll_edge_effect_style = *style;
    }
  }
  return std::move(*this).restate_environment(
      ScrollEnvironment::scroll_edge_effects,
      interactions_.edit().scrolling.edge_effects);
}

auto Element::scroll_edge_effect_hidden(bool hidden,
                                        theme::Edge edges) && -> Element {
  interactions_.edit().scrolling.edge_effects.hide(hidden, edges);
  for (const ToolbarPlacement bars :
       {ToolbarPlacement::navigation_bar, ToolbarPlacement::bottom_bar}) {
    const theme::Edge edge = bars == ToolbarPlacement::navigation_bar
                                 ? theme::Edge::top
                                 : theme::Edge::bottom;
    if ((std::to_underlying(edges) & std::to_underlying(edge)) != 0U) {
      navigation_.edit().chrome.edit_bar(bars).scroll_edge_effect_hidden =
          hidden;
    }
  }
  return std::move(*this).restate_environment(
      ScrollEnvironment::scroll_edge_effects,
      interactions_.edit().scrolling.edge_effects);
}

auto Element::background_extension_effect() && -> Element {
  modifiers_.edit().graphics.extends = true;
  return std::move(*this);
}

auto Element::scroll_content_background(Visibility visibility) && -> Element {
  return std::move(*this).environment(ScrollEnvironment::content_background,
                                      visibility);
}

auto Element::search_focused(FocusBinding<bool> focused) && -> Element {
  return std::move(*this).environment(
      SearchEnvironment::focused,
      std::make_shared<const FocusBinding<bool>>(std::move(focused)));
}

auto Element::scroll_target_layout(bool is_enabled) && -> Element {
  interactions_.edit().scrolling.target_layout = is_enabled;
  return std::move(*this);
}

auto Element::content_margins(theme::Space length,
                              ContentMarginPlacement placement) && -> Element {
  return std::move(*this).content_margins(theme::Insets::all(length),
                                          placement);
}

auto Element::content_margins(theme::Insets insets,
                              ContentMarginPlacement placement) && -> Element {
  if (placement != ContentMarginPlacement::scroll_indicators) {
    interactions_.edit().scrolling.content_margins = insets;
  }
  if (placement != ContentMarginPlacement::scroll_content) {
    interactions_.edit().scrolling.indicator_margins = insets;
  }
  return std::move(*this);
}

auto Element::content_margins(theme::Edge edges, theme::Space length,
                              ContentMarginPlacement placement) && -> Element {
  if (placement != ContentMarginPlacement::scroll_indicators) {
    interactions_.edit().scrolling.content_margins =
        interactions_.edit()
            .scrolling.content_margins.value_or(theme::Insets{})
            .replacing(edges, length);
  }
  if (placement != ContentMarginPlacement::scroll_content) {
    interactions_.edit().scrolling.indicator_margins =
        interactions_.edit()
            .scrolling.indicator_margins.value_or(theme::Insets{})
            .replacing(edges, length);
  }
  return std::move(*this);
}

auto Element::content_margins(
    ContentMarginGuide guide, theme::Edge edges,
    std::optional<Alignment> alignment) && -> Element {
  if (!alignment.has_value()) {
    return std::move(*this).padding(guide.padding(edges));
  }
  return std::move(*this)
      .frame({.max_width = infinity,
              .max_height = infinity,
              .alignment = *alignment})
      .padding(guide.padding(edges));
}

auto Element::safe_area_padding(theme::Space length) && -> Element {
  return std::move(*this).content_margins(length);
}

auto Element::safe_area_padding(theme::Insets insets) && -> Element {
  return std::move(*this).content_margins(insets);
}

auto Element::safe_area_padding(theme::Edge edges,
                                theme::Space length) && -> Element {
  return std::move(*this).content_margins(edges, length);
}

auto Element::scroll_clip_disabled(bool disabled) && -> Element {
  interactions_.edit().scrolling.clip_disabled = disabled;
  return std::move(*this);
}

auto Element::scroll_transition(
    ScrollEffects::Transition transition) && -> Element {
  return std::move(*this).scroll_transition(
      ScrollTransitionConfiguration::interactive(), std::move(transition));
}

auto Element::scroll_transition(
    ScrollTransitionConfiguration configuration,
    ScrollEffects::Transition transition) && -> Element {
  interactions_.edit().effects.transitions.push_back(
      ScrollEffects::ScrollTransition{.configuration = configuration,
                                      .axis = std::nullopt,
                                      .transition = std::move(transition)});
  return std::move(*this);
}

auto Element::scroll_transition(
    ScrollTransitionConfiguration configuration, Axis axis,
    ScrollEffects::Transition transition) && -> Element {
  interactions_.edit().effects.transitions.push_back(
      ScrollEffects::ScrollTransition{.configuration = configuration,
                                      .axis = axis,
                                      .transition = std::move(transition)});
  return std::move(*this);
}

auto Element::scroll_transition(
    ScrollTransitionConfiguration top_leading,
    ScrollTransitionConfiguration bottom_trailing,
    ScrollEffects::Transition transition) && -> Element {
  interactions_.edit().effects.transitions.push_back(
      ScrollEffects::ScrollTransition{.configuration = top_leading,
                                      .axis = std::nullopt,
                                      .transition = std::move(transition),
                                      .bottom_trailing = bottom_trailing});
  return std::move(*this);
}

auto Element::scroll_transition(
    ScrollTransitionConfiguration top_leading,
    ScrollTransitionConfiguration bottom_trailing, Axis axis,
    ScrollEffects::Transition transition) && -> Element {
  interactions_.edit().effects.transitions.push_back(
      ScrollEffects::ScrollTransition{.configuration = top_leading,
                                      .axis = axis,
                                      .transition = std::move(transition),
                                      .bottom_trailing = bottom_trailing});
  return std::move(*this);
}

auto Element::visual_effect(ScrollEffects::Effect effect) && -> Element {
  interactions_.edit().effects.effects.push_back(std::move(effect));
  return std::move(*this);
}

auto Element::geometry_projection(
    ScrollEffects::Projection projection) && -> Element {
  interactions_.edit().effects.projections.push_back(std::move(projection));
  return std::move(*this);
}

auto Element::on_scroll_phase_change(
    Scrolling::PhaseAction action) && -> Element {
  return std::move(*this).on_scroll_phase_change(Scrolling::PhaseContextAction{
      [held = std::move(action)](ScrollPhase before, ScrollPhase after,
                                 const ScrollPhaseChangeContext&) mutable {
        held(before, after);
      }});
}

auto Element::on_scroll_phase_change(
    Scrolling::PhaseContextAction action) && -> Element {
  interactions_.edit().scrolling.phase_actions.push_back(
      std::make_shared<Scrolling::PhaseContextAction>(std::move(action)));
  return std::move(*this);
}

void Element::add_toolbar(ToolbarItem item) {
  navigation_.edit().toolbar.push_back(
      std::make_shared<const ToolbarItem>(std::move(item)));
}

void Element::add_toolbar(ToolbarItemGroup group) {
  for (ToolbarItem& item : std::move(group).items()) {
    add_toolbar(std::move(item));
  }
}

void Element::add_toolbar(ToolbarSpacer spacer) { add_toolbar(spacer.item()); }

void Element::add_toolbar(const std::shared_ptr<const std::string>& toolbar,
                          ToolbarItem item) {
  add_toolbar(std::move(item).customizing(toolbar));
}

void Element::add_toolbar(const std::shared_ptr<const std::string>& toolbar,
                          ToolbarItemGroup group) {
  for (ToolbarItem& item : std::move(group).items()) {
    add_toolbar(toolbar, std::move(item));
  }
}

void Element::add_toolbar(const std::shared_ptr<const std::string>& toolbar,
                          ToolbarSpacer spacer) {
  add_toolbar(toolbar, spacer.item());
}

auto Element::glass_effect(theme::Glass glass,
                           theme::Shape shape) && -> Element {
  GlassEffect effect = modifiers_.edit().glass.value_or(GlassEffect{});
  effect.glass = glass;
  effect.shape = shape;
  modifiers_.edit().glass = effect;
  return std::move(*this);
}

auto Element::glass_effect_id(ElementKey id,
                              Namespace::ID space) && -> Element {
  GlassEffect effect = modifiers_.edit().glass.value_or(GlassEffect{});
  effect.id = GlassEffect::Tag{.space = space, .id = id};
  modifiers_.edit().glass = effect;
  if (effect.transition.kind() !=
      GlassEffectTransition::Kind::matched_geometry) {
    return std::move(*this);
  }
  return std::move(*this).matched_geometry_effect(
      std::move(id), space,
      motion::MatchedGeometryProperties::frame |
          motion::MatchedGeometryProperties::corner_radius);
}

auto Element::glass_effect_transition(
    GlassEffectTransition transition) && -> Element {
  GlassEffect effect = modifiers_.edit().glass.value_or(GlassEffect{});
  effect.transition = transition;
  modifiers_.edit().glass = effect;
  switch (transition.kind()) {
    case GlassEffectTransition::Kind::matched_geometry:
      if (effect.id.has_value() && !motion_->matched.has_value()) {
        return std::move(*this).matched_geometry_effect(
            effect.id->id, effect.id->space,
            motion::MatchedGeometryProperties::frame |
                motion::MatchedGeometryProperties::corner_radius);
      }
      return std::move(*this);
    case GlassEffectTransition::Kind::materialize: {
      if (effect.id.has_value() && motion_.engaged()) {
        motion_.edit().matched.reset();
      }
      const motion::Transition::Effect hidden{
          .opacity = 0.0F, .blur = GlassEffectTransition::materialize_blur};
      return std::move(*this).transition(motion::Transition::effect(hidden));
    }
    case GlassEffectTransition::Kind::identity:
      if (effect.id.has_value() && motion_.engaged()) {
        motion_.edit().matched.reset();
      }
      return std::move(*this).transition(motion::Transition::identity());
  }
  return std::move(*this);
}

auto Element::glass_effect_union(ElementKey id,
                                 Namespace::ID space) && -> Element {
  GlassEffect effect = modifiers_.edit().glass.value_or(GlassEffect{});
  effect.unioned = GlassEffect::Tag{.space = space, .id = std::move(id)};
  modifiers_.edit().glass = effect;
  return std::move(*this);
}

auto Element::default_glass(theme::GlassSlot slot,
                            theme::Shape shape) && -> Element {
  GlassEffect effect = modifiers_.edit().glass.value_or(GlassEffect{});
  effect.slot = slot;
  if (!effect.glass.has_value()) {
    effect.shape = shape;
  }
  modifiers_.edit().glass = effect;
  return std::move(*this);
}

auto Element::toolbar(ToolbarRemoval removal) && -> Element {
  switch (removal.removing) {
    case ToolbarDefaultItemKind::sidebar_toggle:
      return std::move(*this).environment(
          ToolbarRemoval::sidebar_toggle_removed, true);
    case ToolbarDefaultItemKind::search:
      return std::move(*this).environment(ToolbarRemoval::search_removed, true);
    case ToolbarDefaultItemKind::title:
      break;
  }
  return std::move(*this).environment(ToolbarRemoval::title_removed, true);
}

auto Element::search_suggestions(
    Visibility visibility,
    SearchSuggestionsPlacement::Set placements) && -> Element {
  SearchEnvironment::SuggestionVisibility declared;
  if (placements.contains(SearchSuggestionsPlacement::menu)) {
    declared.menu = visibility;
  }
  if (placements.contains(SearchSuggestionsPlacement::content)) {
    declared.content = visibility;
  }
  return std::move(*this).environment(SearchEnvironment::suggestion_visibility,
                                      declared);
}

auto Element::search_presentation_toolbar_behavior(
    SearchPresentationToolbarBehavior behavior) && -> Element {
  return std::move(*this).environment(
      SearchEnvironment::presentation_toolbar_behavior, behavior);
}

auto Element::search_toolbar_behavior(
    SearchToolbarBehavior behavior) && -> Element {
  return std::move(*this).environment(SearchEnvironment::toolbar_behavior,
                                      behavior);
}

auto Element::navigation_link_indicator_visibility(
    Visibility visibility) && -> Element {
  return std::move(*this).environment(
      NavigationDeclarations::link_indicator_visibility, visibility);
}

auto Element::file_dialog_confirmation_label(
    std::optional<std::string> label) && -> Element {
  return std::move(*this).environment(FileDialogEnvironment::confirmation_label,
                                      std::move(label));
}

auto Element::file_dialog_customization_id(std::string id) && -> Element {
  return std::move(*this).environment(
      FileDialogEnvironment::customization_id,
      std::optional<std::string>{std::move(id)});
}

auto Element::file_dialog_default_directory(
    std::optional<FileURL> directory) && -> Element {
  return std::move(*this).environment(FileDialogEnvironment::default_directory,
                                      std::move(directory));
}

auto Element::file_dialog_message(
    std::optional<std::string> message) && -> Element {
  return std::move(*this).environment(FileDialogEnvironment::message,
                                      std::move(message));
}

auto Element::file_dialog_url_enabled(
    std::function<bool(const FileURL&)> predicate) && -> Element {
  return std::move(*this).environment(
      FileDialogEnvironment::url_enabled,
      std::make_shared<const std::function<bool(const FileURL&)>>(
          std::move(predicate)));
}

const EnvironmentKey<std::optional<std::string>>
    FileDialogEnvironment::confirmation_label{"file_dialog_confirmation_label",
                                              std::nullopt};

const EnvironmentKey<std::optional<std::string>>
    FileDialogEnvironment::customization_id{"file_dialog_customization_id",
                                            std::nullopt};

const EnvironmentKey<std::optional<FileURL>>
    FileDialogEnvironment::default_directory{"file_dialog_default_directory",
                                             std::nullopt};

const EnvironmentKey<std::optional<std::string>> FileDialogEnvironment::message{
    "file_dialog_message", std::nullopt};

const EnvironmentKey<FileDialogEnvironment::Enabled>
    FileDialogEnvironment::url_enabled{"file_dialog_url_enabled", nullptr};

auto Element::toolbar_item_hidden(bool hidden) && -> Element {
  if (!hidden) {
    return std::move(*this);
  }
  return ToolbarEnvironment::HiddenItem{std::move(*this)};
}

auto Element::toolbar_title_alignment(
    ToolbarTitleAlignment alignment) && -> Element {
  navigation_.edit().chrome.set_title_alignment(alignment);
  return std::move(*this).environment(ToolbarEnvironment::title_alignment,
                                      alignment);
}

auto Element::toolbar_minimization_behavior(
    ToolbarMinimizationBehavior behavior, ToolbarPlacement bars) && -> Element {
  navigation_.edit().chrome.edit_bar(bars).minimization = behavior;
  return std::move(*this);
}

auto Element::toolbar_minimization_restoration(
    ToolbarMinimizationRestoration restoration,
    ToolbarPlacement bars) && -> Element {
  navigation_.edit().chrome.edit_bar(bars).restoration = restoration;
  return std::move(*this);
}

auto Element::toolbar_minimization_safe_area_adjustment(
    ToolbarMinimizationSafeAreaAdjustment adjustment,
    ToolbarPlacement bars) && -> Element {
  navigation_.edit().chrome.edit_bar(bars).safe_area_adjustment = adjustment;
  return std::move(*this);
}

auto Element::toolbar_vertical_behavior(
    ToolbarVerticalBehavior behavior) && -> Element {
  navigation_.edit().chrome.set_vertical_behavior(behavior);
  return std::move(*this);
}

auto Element::toolbar_vertical_compression_behavior(
    ToolbarVerticalCompressionBehavior behavior) && -> Element {
  navigation_.edit().chrome.set_vertical_compression(behavior);
  return std::move(*this);
}

const EnvironmentKey<bool> ToolbarRemoval::sidebar_toggle_removed{
    "toolbar.sidebar_toggle_removed", false};

const EnvironmentKey<bool> ToolbarRemoval::title_removed{
    "toolbar.title_removed", false};

const EnvironmentKey<bool> ToolbarRemoval::search_removed{
    "toolbar.search_removed", false};

const EnvironmentKey<DialogEnvironment::Icon> DialogEnvironment::icon{
    "dialog_icon", nullptr};

const EnvironmentKey<DialogSeverity> DialogEnvironment::severity{
    "dialog_severity", DialogSeverity::automatic};

const EnvironmentKey<std::optional<PresentationOptions::Suppression>>
    DialogEnvironment::suppression{"dialog_suppression", std::nullopt};

}
