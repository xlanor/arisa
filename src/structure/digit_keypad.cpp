#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/structure/digit_keypad.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <format>
#include <memory>
#include <utility>

namespace arisa::structure {

DigitKeypad::DigitKeypad(Actions actions)
    : DigitKeypad(std::move(actions), Options{}) {}

DigitKeypad::DigitKeypad(Actions actions, Options options)
    : Element(std::make_unique<Content>(std::move(actions),
                                        std::move(options))) {}

auto DigitKeypad::key_of(char digit) -> std::string {
  return std::format("{}.{}", key_prefix, digit);
}

auto DigitKeypad::key_of(std::string_view name) -> std::string {
  return std::format("{}.{}", key_prefix, name);
}

auto DigitKeypad::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto DigitKeypad::Content::key(composition::Element label,
                               std::function<void()> action, std::string id)
    -> composition::Element {
  const theme::Shape shape = theme::Shape::rounded(key_radius, true);
  return components::Button(
             [action = std::move(action)] {
               if (action) {
                 action();
               }
             },
             std::move(label)
                 .frame({.width = key_width, .height = key_height})
                 .background(theme::ColourRole::secondary_system_fill, shape)
                 .content_shape(composition::ContentShapeKinds::interaction,
                                shape))
      .button_style(components::ButtonStyle::plain())
      .content_shape(composition::ContentShapeKinds::focus_effect, shape)
      .id(std::move(id));
}

auto DigitKeypad::Content::digit(char value) const -> composition::Element {
  return key(components::Text(std::string(1, value))
                 .font(theme::Font::system(digit_size,
                                           theme::Font::Weight::medium))
                 .monospaced_digit(),
             [entered = actions_.digit, value] {
               if (entered) {
                 entered(value);
               }
             },
             DigitKeypad::key_of(value));
}

auto DigitKeypad::Content::symbol(const std::string& name,
                                  const std::function<void()>& action,
                                  std::string_view id) const
    -> composition::Element {
  return key(components::Image(components::Image::SystemName{name})
                 .font(theme::Font::system(symbol_size,
                                           theme::Font::Weight::medium)),
             action, DigitKeypad::key_of(id));
}

auto DigitKeypad::Content::body(composition::Context&) const
    -> std::vector<composition::Element> {
  static constexpr std::array<std::array<char, 3>, 3> grid{
      {{'1', '2', '3'}, {'4', '5', '6'}, {'7', '8', '9'}}};
  const theme::Space spacing{key_spacing};
  std::vector<composition::Element> rows;
  rows.reserve(grid.size() + 1);
  for (const std::array<char, 3>& line : grid) {
    rows.push_back(components::HStack(
        components::HStackOptions{.spacing = spacing}, digit(line[0]),
        digit(line[1]), digit(line[2])));
  }
  composition::Element trailing =
      actions_.submit
          ? symbol(options_.submit_symbol, actions_.submit, submit_name)
          : composition::Element{components::ZStack().frame(
                {.width = key_width, .height = key_height})};
  rows.push_back(components::HStack(
      components::HStackOptions{.spacing = spacing},
      symbol(options_.erase_symbol, actions_.erase, erase_name), digit('0'),
      std::move(trailing)));
  std::vector<composition::Element> produced;
  produced.push_back(
      components::VStack(components::VStackOptions{.spacing = spacing},
                         std::move(rows))
          .focus_section());
  return produced;
}

}
