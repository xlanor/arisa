#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_metrics.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/shape.hpp>

#include <utility>

namespace arisa::components {

const composition::EnvironmentKey<ButtonStyle> ButtonStyle::key{
    "button_style", ButtonStyle::automatic()};

auto ButtonStyle::automatic() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::automatic};
}

auto ButtonStyle::bordered() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::bordered};
}

auto ButtonStyle::bordered_prominent() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::bordered_prominent};
}

auto ButtonStyle::borderless() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::borderless};
}

auto ButtonStyle::plain() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::plain};
}

auto ButtonStyle::toolbar() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::toolbar};
}

auto ButtonStyle::glass() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::glass, theme::Glass::regular()};
}

auto ButtonStyle::glass(theme::Glass glass) noexcept -> ButtonStyle {
  return ButtonStyle{Kind::glass, glass};
}

auto ButtonStyle::glass_prominent() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::glass_prominent, theme::Glass::regular()};
}

auto ButtonStyle::accessory_bar() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::accessory_bar};
}

auto ButtonStyle::accessory_bar_action() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::accessory_bar_action};
}

auto ButtonStyle::link() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::link};
}

auto ButtonStyle::card() noexcept -> ButtonStyle {
  return ButtonStyle{Kind::card};
}

auto operator==(const ButtonStyle& left, const ButtonStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  if (left.kind_ == ButtonStyle::Kind::glass) {
    return left.glass_ == right.glass_;
  }
  if (left.kind_ != ButtonStyle::Kind::custom) {
    return true;
  }
  return left.custom_->equals(*right.custom_);
}

auto ButtonStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::bordered_prominent:
      return filled(std::move(configuration), true);
    case Kind::automatic:
    case Kind::bordered:
      return filled(std::move(configuration), false);
    case Kind::borderless:
      return bare(std::move(configuration), true);
    case Kind::plain:
      return bare(std::move(configuration), false);
    case Kind::toolbar:
      return tooled(std::move(configuration));
    case Kind::glass:
      return glazed(std::move(configuration), glass_, false);
    case Kind::glass_prominent:
      return glazed(std::move(configuration), glass_, true);
    case Kind::accessory_bar:
      return accessory(std::move(configuration), false);
    case Kind::accessory_bar_action:
      return accessory(std::move(configuration), true);
    case Kind::link:
      return linked(std::move(configuration));
    case Kind::card:
      return carded(std::move(configuration));
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return std::move(configuration.label);
}

auto ButtonStyle::filled(Configuration configuration, bool prominent)
    -> composition::Element {
  const ControlMetrics metrics = ControlMetrics::of(configuration.control_size);
  const float vertical = metrics.button_vertical_padding;
  const bool closes =
      configuration.role == ButtonRole::close &&
      configuration.border_shape.kind() == ButtonBorderShape::Kind::automatic;
  const theme::Shape shape =
      closes ? theme::Shape::capsule()
             : configuration.border_shape.shape(configuration.control_size);
  const bool circle = closes || configuration.border_shape.kind() ==
                                    ButtonBorderShape::Kind::circle;
  prominent = prominent || configuration.role == ButtonRole::confirm;
  const theme::Insets insets =
      configuration.control_size == ControlSize::regular && !circle
          ? Button::label_insets
          : theme::Insets::symmetric(
                theme::Space{circle ? vertical
                                    : metrics.button_horizontal_padding},
                theme::Space{vertical});
  composition::Element body = flexed(
      VStack(std::move(configuration.label)).padding(insets), configuration);
  if (prominent) {
    body = std::move(body)
               .background(configuration.is_pressed
                               ? theme::ColourRole::accent_pressed
                               : theme::ColourRole::accent,
                           shape)
               .foreground_style(theme::ColourRole::on_accent);
  } else {
    body = std::move(body)
               .background(fill(
                   configuration.is_pressed ? pressed_fill_role : fill_role,
                   shape))
               .foreground_style(theme::ColourRole::control_text);
  }
  return finish(std::move(body), configuration);
}

auto ButtonStyle::glazed(Configuration configuration, theme::Glass glass,
                         bool prominent) -> composition::Element {
  const theme::Glass surface =
      prominent ? glass.tint(theme::ColourRole::accent).interactive()
                : glass.interactive();
  const ControlMetrics metrics = ControlMetrics::of(configuration.control_size);
  const theme::Shape shape =
      configuration.border_shape.kind() == ButtonBorderShape::Kind::automatic
          ? theme::Shape::capsule()
          : configuration.border_shape.shape(configuration.control_size);
  const theme::Insets insets =
      configuration.control_size == ControlSize::regular
          ? Button::label_insets
          : theme::Insets::symmetric(
                theme::Space{metrics.button_horizontal_padding},
                theme::Space{metrics.button_vertical_padding});
  composition::Element body = flexed(
      VStack(std::move(configuration.label)).padding(insets), configuration);
  body = std::move(body)
             .glass_effect(surface, shape)
             .foreground_style(prominent ? theme::ColourRole::on_accent
                                         : theme::ColourRole::control_text);
  if (configuration.is_pressed) {
    body = std::move(body).opacity(prominent ? prominent_pressed_opacity
                                             : pressed_opacity);
  }
  return finish(std::move(body), configuration);
}

auto ButtonStyle::fill(theme::ColourRole role) -> composition::Element {
  return fill(role, theme::Shape::rounded(theme::radius::control, true));
}

auto ButtonStyle::fill(theme::ColourRole role, theme::Shape shape)
    -> composition::Element {
  return ZStack()
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity})
      .background(role, shape)
      .allows_hit_testing(false);
}

auto ButtonStyle::bare(Configuration configuration, bool tinted)
    -> composition::Element {
  composition::Element body =
      flexed(VStack(std::move(configuration.label)), configuration);
  if (configuration.shows_shape) {
    body = std::move(body)
               .padding(theme::Insets::symmetric(theme::space::sm,
                                                 theme::space::xxs))
               .background(fill(
                   configuration.is_pressed ? pressed_fill_role : fill_role,
                   theme::Shape::capsule()));
  }
  if (tinted) {
    body = std::move(body).foreground_style(theme::ColourRole::text_secondary);
  }
  if (configuration.is_pressed) {
    body = std::move(body).opacity(pressed_opacity);
  }
  return finish(std::move(body), configuration);
}

auto ButtonStyle::tooled(Configuration configuration) -> composition::Element {
  composition::Element body =
      flexed(VStack(std::move(configuration.label))
                 .padding(theme::Insets::symmetric(
                     theme::metric::toolbar_horizontal_padding,
                     theme::metric::toolbar_vertical_padding)),
             configuration)
          .foreground_style(theme::ColourRole::control_text);
  if (configuration.is_pressed || configuration.shows_shape) {
    body = std::move(body).background(
        configuration.is_pressed ? pressed_fill_role : fill_role,
        theme::Shape::rounded(theme::radius::control, true));
  }
  return finish(std::move(body), configuration);
}

auto ButtonStyle::accessory(Configuration configuration, bool action)
    -> composition::Element {
  composition::Element body =
      flexed(VStack(std::move(configuration.label))
                 .padding(theme::Insets::symmetric(
                     theme::Space{accessory_horizontal_padding},
                     theme::Space{accessory_vertical_padding})),
             configuration)
          .foreground_style(action ? theme::ColourRole::control_text
                                   : theme::ColourRole::text_secondary);
  if (action || configuration.is_pressed || configuration.shows_shape) {
    body = std::move(body).background(
        fill(configuration.is_pressed ? pressed_fill_role : fill_role,
             theme::Shape::rounded(theme::radius::control, true)));
  }
  return finish(std::move(body), configuration);
}

auto ButtonStyle::linked(Configuration configuration) -> composition::Element {
  composition::Element body =
      flexed(VStack(std::move(configuration.label)), configuration)
          .foreground_style(theme::ColourRole::link);
  if (configuration.is_pressed) {
    body = std::move(body).opacity(pressed_opacity);
  }
  return finish(std::move(body), configuration);
}

auto ButtonStyle::carded(Configuration configuration) -> composition::Element {
  const float lift = configuration.is_pressed   ? card_pressed_scale
                     : configuration.is_focused ? card_focus_scale
                                                : 1.0F;
  composition::Element body =
      flexed(VStack(std::move(configuration.label))
                 .padding(theme::Space{card_padding}),
             configuration)
          .background(theme::ColourRole::secondary_system_background,
                      theme::Shape::rounded(card_radius, true))
          .foreground_style(theme::ColourRole::label)
          .shadow(theme::Colour{0.0F, 0.0F, 0.0F,
                                configuration.is_focused ? 0.35F : 0.15F},
                  configuration.is_focused ? 16.0F : 4.0F, 0.0F,
                  configuration.is_focused ? 10.0F : 2.0F)
          .scale_effect(lift)
          .animation(motion::Animation::snappy(0.25), configuration.is_focused);
  return finish(std::move(body), configuration);
}

auto ButtonStyle::flexed(composition::Element element,
                         const Configuration& configuration)
    -> composition::Element {
  if (configuration.sizing != ButtonSizing::flexible) {
    return element;
  }
  return std::move(element).frame({.max_width = composition::infinity});
}

auto ButtonStyle::finish(composition::Element element,
                         const Configuration& configuration)
    -> composition::Element {
  if (!configuration.is_enabled) {
    element = std::move(element).opacity(disabled_opacity);
  }
  return std::move(element).animation(motion::Animation::snappy(0.2),
                                      configuration.is_pressed);
}

ButtonStyle::ButtonStyle(AutomaticButtonStyle) noexcept
    : kind_(Kind::automatic) {}

auto AutomaticButtonStyle::make_body(
    ButtonStyle::Configuration configuration) const -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(BorderedButtonStyle) noexcept
    : kind_(Kind::bordered) {}

auto BorderedButtonStyle::make_body(
    ButtonStyle::Configuration configuration) const -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(BorderedProminentButtonStyle) noexcept
    : kind_(Kind::bordered_prominent) {}

auto BorderedProminentButtonStyle::make_body(
    ButtonStyle::Configuration configuration) const -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(BorderlessButtonStyle) noexcept
    : kind_(Kind::borderless) {}

auto BorderlessButtonStyle::make_body(
    ButtonStyle::Configuration configuration) const -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(PlainButtonStyle) noexcept : kind_(Kind::plain) {}

auto PlainButtonStyle::make_body(ButtonStyle::Configuration configuration) const
    -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(GlassButtonStyle style) noexcept
    : kind_(Kind::glass), glass_(style.glass()) {}

auto GlassButtonStyle::make_body(ButtonStyle::Configuration configuration) const
    -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(GlassProminentButtonStyle) noexcept
    : kind_(Kind::glass_prominent) {}

auto GlassProminentButtonStyle::make_body(
    ButtonStyle::Configuration configuration) const -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(AccessoryBarButtonStyle) noexcept
    : kind_(Kind::accessory_bar) {}

auto AccessoryBarButtonStyle::make_body(
    ButtonStyle::Configuration configuration) const -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(AccessoryBarActionButtonStyle) noexcept
    : kind_(Kind::accessory_bar_action) {}

auto AccessoryBarActionButtonStyle::make_body(
    ButtonStyle::Configuration configuration) const -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(LinkButtonStyle) noexcept : kind_(Kind::link) {}

auto LinkButtonStyle::make_body(ButtonStyle::Configuration configuration) const
    -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

ButtonStyle::ButtonStyle(CardButtonStyle) noexcept : kind_(Kind::card) {}

auto CardButtonStyle::make_body(ButtonStyle::Configuration configuration) const
    -> composition::Element {
  return ButtonStyle{*this}.make_body(std::move(configuration));
}

}
