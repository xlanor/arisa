#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_metrics.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>

#include <utility>

namespace arisa::controls {

const composition::EnvironmentKey<ToggleStyle> ToggleStyle::key{
    "toggle_style", ToggleStyle::automatic()};

auto ToggleStyle::automatic() noexcept -> ToggleStyle {
  return ToggleStyle{Kind::automatic};
}

auto ToggleStyle::checkbox() noexcept -> ToggleStyle {
  return ToggleStyle{Kind::checkbox};
}

auto ToggleStyle::button() noexcept -> ToggleStyle {
  return ToggleStyle{Kind::button};
}

auto operator==(const ToggleStyle& left, const ToggleStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != ToggleStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto ToggleStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::automatic:
    case Kind::switch_toggle:
      return switched(std::move(configuration));
    case Kind::checkbox:
      return checked(std::move(configuration));
    case Kind::button:
      return pushed(std::move(configuration));
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return std::move(configuration.label);
}

auto ToggleStyle::track(const Configuration& configuration)
    -> composition::Element {
  const components::ControlMetrics metrics =
      components::ControlMetrics::of(configuration.control_size);
  const float travel = (metrics.switch_width - (metrics.knob_inset * 2.0F) -
                        metrics.knob_width) *
                       0.5F;
  return components::ZStack(
             components::ZStack()
                 .frame({.width = metrics.knob_width,
                         .height = metrics.knob_height})
                 .background(theme::ColourRole::highlight,
                             theme::Shape::capsule())
                 .border(theme::ColourRole::separator,
                         theme::Stroke::hairline())
                 .shadow(theme::Colour{0.0F, 0.0F, 0.0F, knob_shadow_opacity},
                         knob_shadow_radius, 0.0F, 1.0F)
                 .scale_effect(configuration.is_pressed ? 0.92F : 1.0F)
                 .offset(configuration.is_mixed ? 0.0F
                         : configuration.is_on  ? travel
                                                : -travel))
      .frame({.width = metrics.switch_width, .height = metrics.switch_height})
      .background(
          configuration.is_mixed
              ? theme::ColourValue{theme::ColourRole::accent}.opacity(0.5F)
          : configuration.is_on
              ? theme::ColourValue{theme::ColourRole::accent}
              : theme::ColourValue{theme::ColourRole::system_fill},
          theme::Shape::capsule());
}

auto ToggleStyle::box(const Configuration& configuration)
    -> composition::Element {
  const float extent =
      components::ControlMetrics::of(configuration.control_size)
          .checkbox_extent;
  composition::Element square =
      configuration.is_mixed
          ? composition::Element{components::ZStack(
                                     components::Glyph::minus(
                                         theme::ColourRole::on_accent,
                                         extent * 0.7F))
                                     .background(
                                         theme::ColourRole::accent,
                                         theme::Shape::rounded(box_radius))}
      : configuration.is_on
          ? composition::Element{components::ZStack(
                                     components::Glyph::checkmark(
                                         theme::ColourRole::on_accent,
                                         extent * 0.8F))
                                     .background(
                                         theme::ColourRole::accent,
                                         theme::Shape::rounded(box_radius))}
          : composition::Element{components::ZStack().background(
                components::ButtonStyle::fill_role,
                theme::Shape::rounded(box_radius))};
  return std::move(square).frame({.width = extent, .height = extent});
}

auto ToggleStyle::switched(Configuration configuration)
    -> composition::Element {
  if (components::Group::is_empty(configuration.label)) {
    return finish(track(configuration), configuration);
  }
  composition::Element row =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = std::nullopt,
              .distribution = components::Distribution::start,
              .wrap = false},
          std::move(configuration.label),
          components::Spacer(theme::metrics::toggle_switch.label_gap),
          track(configuration))
          .frame({.max_width = composition::infinity});
  return finish(std::move(row), configuration);
}

auto ToggleStyle::checked(Configuration configuration) -> composition::Element {
  if (components::Group::is_empty(configuration.label)) {
    return finish(box(configuration), configuration);
  }
  composition::Element row =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::metrics::checkbox.label_gap,
              .distribution = components::Distribution::start,
              .wrap = false},
          box(configuration), std::move(configuration.label),
          components::Spacer())
          .frame({.max_width = composition::infinity});
  return finish(std::move(row), configuration);
}

auto ToggleStyle::pushed(Configuration configuration) -> composition::Element {
  const bool on = configuration.is_on;
  const components::ControlMetrics metrics =
      components::ControlMetrics::of(configuration.control_size);
  composition::Element body =
      components::VStack(std::move(configuration.label))
          .padding(configuration.control_size ==
                           components::ControlSize::regular
                       ? components::Button::label_insets
                       : theme::Insets::symmetric(
                             theme::Space{metrics.button_horizontal_padding},
                             theme::Space{metrics.button_vertical_padding}));
  if (on) {
    body = std::move(body)
               .background(theme::ColourRole::accent,
                           theme::Shape::rounded(theme::radius::control, true))
               .foreground_style(theme::ColourRole::on_accent);
  } else {
    body = std::move(body)
               .background(configuration.is_pressed
                               ? components::ButtonStyle::pressed_fill_role
                               : components::ButtonStyle::fill_role,
                           theme::Shape::rounded(theme::radius::control, true))
               .foreground_style(theme::ColourRole::control_text);
  }
  if (configuration.is_pressed) {
    body = std::move(body).opacity(
        components::ButtonStyle::prominent_pressed_opacity);
  }
  return finish(std::move(body), configuration);
}

auto ToggleStyle::finish(composition::Element element,
                         const Configuration& configuration)
    -> composition::Element {
  if (!configuration.is_enabled) {
    element =
        std::move(element).opacity(components::ButtonStyle::disabled_opacity);
  }
  return std::move(element)
      .animation(motion::Animation::snappy(0.25), configuration.is_on)
      .animation(motion::Animation::snappy(0.2), configuration.is_pressed);
}

ToggleStyle::ToggleStyle(DefaultToggleStyle) noexcept
    : kind_(Kind::automatic) {}

auto DefaultToggleStyle::make_body(
    ToggleStyle::Configuration configuration) const -> composition::Element {
  return ToggleStyle{*this}.make_body(std::move(configuration));
}

ToggleStyle::ToggleStyle(SwitchToggleStyle) noexcept
    : kind_(Kind::switch_toggle) {}

auto SwitchToggleStyle::make_body(
    ToggleStyle::Configuration configuration) const -> composition::Element {
  return ToggleStyle{*this}.make_body(std::move(configuration));
}

ToggleStyle::ToggleStyle(CheckboxToggleStyle) noexcept
    : kind_(Kind::checkbox) {}

auto CheckboxToggleStyle::make_body(
    ToggleStyle::Configuration configuration) const -> composition::Element {
  return ToggleStyle{*this}.make_body(std::move(configuration));
}

ToggleStyle::ToggleStyle(ButtonToggleStyle) noexcept : kind_(Kind::button) {}

auto ButtonToggleStyle::make_body(
    ToggleStyle::Configuration configuration) const -> composition::Element {
  return ToggleStyle{*this}.make_body(std::move(configuration));
}

}
