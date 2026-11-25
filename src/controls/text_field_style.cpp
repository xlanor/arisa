#include <arisa/components/button_style.hpp>
#include <arisa/components/control_metrics.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/text_field_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>

#include <utility>

namespace arisa::controls {

const composition::EnvironmentKey<TextFieldStyle> TextFieldStyle::key{
    "text_field_style", TextFieldStyle::automatic()};

auto TextFieldStyle::automatic() noexcept -> TextFieldStyle {
  return TextFieldStyle{Kind::automatic};
}

auto TextFieldStyle::plain() noexcept -> TextFieldStyle {
  return TextFieldStyle{Kind::plain};
}

auto TextFieldStyle::rounded_border() noexcept -> TextFieldStyle {
  return TextFieldStyle{Kind::rounded_border};
}

auto TextFieldStyle::bordered() noexcept -> TextFieldStyle {
  return TextFieldStyle{Kind::bordered};
}

auto TextFieldStyle::square_border() noexcept -> TextFieldStyle {
  return TextFieldStyle{Kind::square_border};
}

auto operator==(const TextFieldStyle& left, const TextFieldStyle& right)
    -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != TextFieldStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto TextFieldStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::automatic:
    case Kind::rounded_border:
    case Kind::bordered:
      return boxed(std::move(configuration));
    case Kind::square_border:
      configuration.border_shape = TextInputBorderShape::rounded_rectangle();
      return boxed(std::move(configuration));
    case Kind::plain:
      return bare(std::move(configuration));
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return bare(std::move(configuration));
}

auto TextFieldStyle::boxed(Configuration configuration)
    -> composition::Element {
  const bool focused = configuration.is_focused;
  const bool sized =
      configuration.control_size != components::ControlSize::regular;
  const components::ControlMetrics metrics =
      components::ControlMetrics::of(configuration.control_size);
  composition::Element label = std::move(configuration.label);
  if (sized) {
    label = std::move(label).font(theme::Font::body.scaled(metrics.font_scale));
  }
  composition::Element body =
      components::HStack(std::move(label), components::Spacer())
          .padding(theme::Insets::symmetric(
              theme::metric::text_field_inset,
              sized ? theme::Space{0.0F}
                    : theme::Space{theme::metric::text_field_vertical_inset}))
          .frame(
              {.max_width = composition::infinity,
               .min_height =
                   sized
                       ? composition::Length{metrics.text_field_height}
                       : composition::Length{theme::metric::text_field_height}})
          .background(theme::ColourRole::text_background,
                      configuration.border_shape.shape())
          .border(focused ? theme::ColourRole::accent
                          : theme::ColourRole::separator,
                  focused ? theme::Stroke{2.0F} : theme::Stroke::hairline());
  if (!configuration.is_enabled) {
    body = std::move(body).opacity(components::ButtonStyle::disabled_opacity);
  }
  return std::move(body).animation(motion::Animation::snappy(0.2), focused);
}

auto TextFieldStyle::bare(Configuration configuration) -> composition::Element {
  composition::Element body =
      components::HStack(std::move(configuration.label), components::Spacer())
          .frame({.max_width = composition::infinity});
  if (configuration.is_pressed) {
    body = std::move(body).opacity(components::ButtonStyle::pressed_opacity);
  }
  if (!configuration.is_enabled) {
    body = std::move(body).opacity(components::ButtonStyle::disabled_opacity);
  }
  return body;
}

TextFieldStyle::TextFieldStyle(DefaultTextFieldStyle) noexcept
    : kind_(Kind::automatic) {}

auto DefaultTextFieldStyle::make_body(
    TextFieldStyle::Configuration configuration) const -> composition::Element {
  return TextFieldStyle{*this}.make_body(std::move(configuration));
}

TextFieldStyle::TextFieldStyle(PlainTextFieldStyle) noexcept
    : kind_(Kind::plain) {}

auto PlainTextFieldStyle::make_body(
    TextFieldStyle::Configuration configuration) const -> composition::Element {
  return TextFieldStyle{*this}.make_body(std::move(configuration));
}

TextFieldStyle::TextFieldStyle(RoundedBorderTextFieldStyle) noexcept
    : kind_(Kind::rounded_border) {}

auto RoundedBorderTextFieldStyle::make_body(
    TextFieldStyle::Configuration configuration) const -> composition::Element {
  return TextFieldStyle{*this}.make_body(std::move(configuration));
}

TextFieldStyle::TextFieldStyle(BorderedTextFieldStyle) noexcept
    : kind_(Kind::bordered) {}

auto BorderedTextFieldStyle::make_body(
    TextFieldStyle::Configuration configuration) const -> composition::Element {
  return TextFieldStyle{*this}.make_body(std::move(configuration));
}

TextFieldStyle::TextFieldStyle(SquareBorderTextFieldStyle) noexcept
    : kind_(Kind::square_border) {}

auto SquareBorderTextFieldStyle::make_body(
    TextFieldStyle::Configuration configuration) const -> composition::Element {
  return TextFieldStyle{*this}.make_body(std::move(configuration));
}

}
