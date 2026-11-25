#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/structure/control_group_style.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>

#include <string>
#include <utility>
#include <vector>

namespace arisa::structure {

const composition::EnvironmentKey<ControlGroupStyle> ControlGroupStyle::key{
    "control_group_style", ControlGroupStyle::automatic()};

auto ControlGroupStyle::automatic() noexcept -> ControlGroupStyle {
  return ControlGroupStyle{Kind::automatic};
}

auto ControlGroupStyle::navigation() noexcept -> ControlGroupStyle {
  return ControlGroupStyle{Kind::navigation};
}

auto ControlGroupStyle::palette() noexcept -> ControlGroupStyle {
  return ControlGroupStyle{Kind::palette};
}

auto ControlGroupStyle::menu() noexcept -> ControlGroupStyle {
  return ControlGroupStyle{Kind::menu};
}

auto ControlGroupStyle::compact_menu() noexcept -> ControlGroupStyle {
  return ControlGroupStyle{Kind::compact_menu};
}

auto operator==(const ControlGroupStyle& left, const ControlGroupStyle& right)
    -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != ControlGroupStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto ControlGroupStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::automatic:
    case Kind::navigation:
      return joined(std::move(configuration));
    case Kind::palette:
      return spread(std::move(configuration));
    case Kind::menu:
      return menued(std::move(configuration), false);
    case Kind::compact_menu:
      return menued(std::move(configuration), true);
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return joined(std::move(configuration));
}

auto ControlGroupStyle::SegmentStyle::make_body(
    components::ButtonStyle::Configuration configuration) const
    -> composition::Element {
  composition::Element body =
      components::VStack(std::move(configuration.label))
          .padding(components::Button::label_insets)
          .foreground_style(theme::ColourRole::control_text);
  if (configuration.is_pressed) {
    body = std::move(body).background(
        components::ButtonStyle::pressed_fill_role,
        theme::Shape::rounded(theme::radius::control, true));
  }
  if (!configuration.is_enabled) {
    body = std::move(body).opacity(components::ButtonStyle::disabled_opacity);
  }
  return body;
}

auto ControlGroupStyle::joined(Configuration configuration)
    -> composition::Element {
  std::vector<composition::Element> cells;
  bool first = true;
  for (composition::Element& member :
       components::Group::members(configuration.content())) {
    if (!first) {
      cells.push_back(components::ZStack()
                          .frame({.width = 1.0F, .height = divider_height})
                          .background(theme::ColourRole::separator,
                                      theme::Shape::rectangle()));
    }
    first = false;
    cells.push_back(std::move(member).button_style(SegmentStyle{}));
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::space::none},
             std::move(cells))
      .background(
          components::ButtonStyle::fill(components::ButtonStyle::fill_role));
}

auto ControlGroupStyle::spread(Configuration configuration)
    -> composition::Element {
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::space::sm},
             components::Group::members(configuration.content()))
      .button_style(components::ButtonStyle::borderless());
}

auto ControlGroupStyle::menued(Configuration configuration, bool compact)
    -> composition::Element {
  composition::Element label =
      compact || components::Group::is_empty(configuration.label)
          ? composition::Element{components::Image::system_name(
                std::string{overflow_symbol})}
          : std::move(configuration.label);
  return controls::Menu(std::move(configuration.content), std::move(label))
      .menu_indicator(compact ? composition::Visibility::hidden
                              : composition::Visibility::visible);
}

}
