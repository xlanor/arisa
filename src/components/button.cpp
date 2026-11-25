#include <arisa/async/awaitables.hpp>
#include <arisa/async/task.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/control_metrics.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/label_style.hpp>
#include <arisa/components/primitive_button_style.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/enablement.hpp>

#include <memory>
#include <string>
#include <utility>

namespace arisa::components {

Button::Button(std::string label, Action action)
    : Button(std::move(label), std::nullopt, std::move(action)) {}

Button::Button(std::string label, std::optional<ButtonRole> role, Action action)
    : Element(std::make_unique<Content>(std::move(label), role,
                                        std::move(action), nullptr)) {}

Button::Button(Action action, composition::Element label)
    : Button(std::nullopt, std::move(action), std::move(label)) {}

Button::Button(std::optional<ButtonRole> role, Action action,
               composition::Element label)
    : Element(std::make_unique<Content>(
          std::string{}, role, std::move(action),
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(label)))) {}

Button::Button(std::optional<ButtonRole> role, Action action)
    : Button(role, std::move(action), system_label(role)) {}

Button::Button(std::string title, Image::SystemName system_image, Action action)
    : Button(std::move(title), std::move(system_image), std::nullopt,
             std::move(action)) {}

Button::Button(std::string title, Image::SystemName system_image,
               std::optional<ButtonRole> role, Action action)
    : Button(role, std::move(action),
             Label(std::move(title), Image{std::move(system_image)})) {}

Button::Button(std::string title, Image::Resource image, Action action)
    : Button(std::move(title), std::move(image), std::nullopt,
             std::move(action)) {}

Button::Button(std::string title, Image::Resource image,
               std::optional<ButtonRole> role, Action action)
    : Button(role, std::move(action),
             Label(std::move(title), Image{std::move(image)})) {}

auto Button::system_label(std::optional<ButtonRole> role)
    -> composition::Element {
  if (!role.has_value()) {
    return Group();
  }
  switch (*role) {
    case ButtonRole::close:
      return Label("Close", Image::system_name("xmark"))
          .label_style(LabelStyle::icon_only());
    case ButtonRole::confirm:
      return Label("Done", Image::system_name("checkmark"))
          .label_style(LabelStyle::icon_only());
    case ButtonRole::cancel:
      return Text(text::LocalizedStringKey{"Cancel"});
    case ButtonRole::destructive:
      return Text(text::LocalizedStringKey{"Delete"});
  }
  return Group();
}

auto Button::content(composition::Element& element)
    -> std::optional<std::reference_wrapper<Content>> {
  if (element.type() != composition::ElementType::of<Content>()) {
    return std::nullopt;
  }
  return std::ref(static_cast<Content&>(content_of(element)));
}

void Button::Content::then(Action after) {
  action_ = [first = std::move(action_), second = std::move(after)]() mutable {
    if (first) {
      first();
    }
    if (second) {
      second();
    }
  };
}

auto Button::Content::shared_action() -> std::shared_ptr<Action> {
  return relay();
}

auto Button::Content::relay() const -> std::shared_ptr<Action> {
  if (relay_ == nullptr) {
    relay_ = std::make_shared<Action>(std::move(action_));
    action_ = [shared = relay_] {
      if (*shared) {
        (*shared)();
      }
    };
  }
  return relay_;
}

auto Button::Content::same_as(const composition::ElementContent& other) const
    -> bool {
  if (!paints_like(other)) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return static_cast<bool>(action_) == static_cast<bool>(theirs.action_) &&
         (source_ == nullptr) == (theirs.source_ == nullptr);
}

auto Button::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return label_ == theirs.label_ && role_ == theirs.role_ &&
         discloses_ == theirs.discloses_;
}

auto Button::Content::adopt(composition::ElementContent& incoming) -> bool {
  auto& newer = static_cast<Content&>(incoming);
  if (relay_ != nullptr) {
    *relay_ = std::move(newer.action_);
  } else {
    action_ = std::move(newer.action_);
  }
  if (source_ == nullptr || newer.source_ == nullptr) {
    return false;
  }
  source_->forward(*newer.source_);
  return true;
}

auto Button::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Button::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const ButtonStyle& style = context.environment(ButtonStyle::key);
  const std::optional<PrimitiveButtonStyle>& primitive =
      context.environment(PrimitiveButtonStyle::key);
  const ControlSize size = context.environment(Environment::control_size);
  const ButtonBorderShape& border =
      context.environment(Environment::button_border_shape);
  const ButtonSizing sizing = context.environment(Environment::button_sizing);
  const bool repeats =
      context.environment(Environment::button_repeat_behavior) ==
      ButtonRepeatBehavior::enabled;
  takes_over_ = primitive.has_value() || repeats;
  automatic_ = style.is_automatic() && !takes_over_ &&
               role_ != ButtonRole::confirm && role_ != ButtonRole::close &&
               size == ControlSize::regular &&
               border.kind() == ButtonBorderShape::Kind::automatic &&
               sizing != ButtonSizing::flexible;
  std::vector<composition::Element> produced;
  if (automatic_) {
    if (source_ != nullptr) {
      produced.emplace_back(composition::PlaceholderContentView(source_));
    }
    return produced;
  }
  composition::Element label =
      source_ != nullptr
          ? composition::Element{composition::PlaceholderContentView(source_)}
          : composition::Element{
                Text(label_).font(theme::Font{label_role}.scaled(
                    ControlMetrics::of(size).font_scale))};
  if (source_ != nullptr && size != ControlSize::regular) {
    label = std::move(label).font(
        theme::Font{label_role}.scaled(ControlMetrics::of(size).font_scale));
  }
  if (primitive.has_value()) {
    const std::shared_ptr<Action> shared = relay();
    produced.push_back(primitive
                           ->make_body(PrimitiveButtonStyle::Configuration{
                               .label = std::move(label),
                               .role = role_,
                               .trigger =
                                   [shared] {
                                     if (*shared) {
                                       (*shared)();
                                     }
                                   }})
                           .focusable());
    return produced;
  }
  composition::Element body = style.make_body(ButtonStyle::Configuration{
      .label = std::move(label),
      .is_pressed = context.is_pressed(),
      .is_focused = context.is_focused(),
      .is_enabled = context.environment(composition::Enablement::is_enabled),
      .role = role_,
      .control_size = size,
      .border_shape = border,
      .sizing = sizing,
      .shows_shape =
          context.environment(Environment::accessibility_show_button_shapes)});
  if (repeats) {
    const std::shared_ptr<Action> shared = relay();
    const auto fire = [shared] {
      if (*shared) {
        (*shared)();
      }
    };
    body = std::move(body).focusable().on_command(
        CommandMapping::select,
        label_.empty() ? std::string{CommandMapping::select_label} : label_,
        fire,
        composition::CommandOptions{
            .trigger = composition::CommandTrigger::repeat, .hidden = false});
    body = repeating(context, std::move(body), fire);
  }
  produced.push_back(std::move(body));
  return produced;
}

auto Button::Content::repeating(composition::Context& context,
                                composition::Element body,
                                const std::function<void()>& fire)
    -> composition::Element {
  const auto holding = context.state(held);
  const auto fired = context.state(repeated);
  const bool down = holding.get();
  return std::move(body)
      .gesture(composition::DragGesture(0.0F)
                   .on_changed([holding,
                                fired](const composition::DragGesture::Value&) {
                     if (!holding.get()) {
                       fired.set(false);
                       holding.set(true);
                     }
                   })
                   .on_ended([holding, fired,
                              fire](const composition::DragGesture::Value&) {
                     if (!fired.get()) {
                       fire();
                     }
                     holding.set(false);
                     fired.set(false);
                   }))
      .task(composition::ElementKey{down ? 1 : 0},
            [down, fired, fire] { return repeat(down, fired, fire); });
}

auto Button::Content::repeat(bool down, composition::StateHandle<bool> fired,
                             std::function<void()> fire) -> async::Task<void> {
  if (!down) {
    co_return;
  }
  co_await async::sleep(repeat_delay);
  while (!(co_await async::stop_requested())) {
    fire();
    fired.set(true);
    co_await async::sleep(repeat_interval);
  }
}

auto Button::Content::localized_label(const text::StringCatalog& catalog,
                                      const text::Locale& locale) const
    -> std::string_view {
  if (catalog.empty() || label_.empty()) {
    return label_;
  }
  if (!localized_.has_value() || localized_->locale != locale.tag() ||
      localized_->catalog != catalog) {
    localized_ = Localized{
        .locale = std::string{locale.tag()},
        .catalog = catalog,
        .text = catalog.resolve(text::LocalizedStringKey{label_}, locale)};
  }
  return localized_->text;
}

auto Button::Content::accessibility() const
    -> composition::AccessibilityDefaults {
  return composition::AccessibilityDefaults{
      .label = label_.empty() ? std::nullopt : std::optional{label_},
      .value = std::nullopt,
      .traits = composition::AccessibilityTraits{
          composition::AccessibilityTrait::is_button}};
}

auto Button::Content::activatable() const noexcept -> bool {
  return static_cast<bool>(action_) && !takes_over_;
}

auto Button::Content::activate() -> bool {
  if (!action_) {
    return false;
  }
  action_();
  return true;
}

}
