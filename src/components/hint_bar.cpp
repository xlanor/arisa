#include <arisa/components/environment.hpp>
#include <arisa/components/hint_bar.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <string>
#include <utility>
#include <vector>

namespace arisa::components {

auto HintBar::glyph(provider::Button button) noexcept -> std::string_view {
  switch (button) {
    case provider::Button::a:
      return "";
    case provider::Button::b:
      return "";
    case provider::Button::x:
      return "";
    case provider::Button::y:
      return "";
    case provider::Button::l:
      return "";
    case provider::Button::r:
      return "";
    case provider::Button::zl:
      return "";
    case provider::Button::zr:
      return "";
    case provider::Button::plus:
      return "";
    case provider::Button::minus:
      return "";
    case provider::Button::dpad_up:
      return "";
    case provider::Button::dpad_down:
      return "";
    case provider::Button::dpad_left:
      return "";
    case provider::Button::dpad_right:
      return "";
    case provider::Button::left_stick:
      return "";
    case provider::Button::right_stick:
      return "";
    case provider::Button::sl:
    case provider::Button::sr:
      return {};
  }
  return "";
}

auto HintBar::name(provider::Button button) noexcept -> std::string_view {
  switch (button) {
    case provider::Button::a:
      return "A";
    case provider::Button::b:
      return "B";
    case provider::Button::x:
      return "X";
    case provider::Button::y:
      return "Y";
    case provider::Button::l:
      return "L";
    case provider::Button::r:
      return "R";
    case provider::Button::zl:
      return "ZL";
    case provider::Button::zr:
      return "ZR";
    case provider::Button::plus:
      return "+";
    case provider::Button::minus:
      return "-";
    case provider::Button::dpad_up:
      return "Up";
    case provider::Button::dpad_down:
      return "Down";
    case provider::Button::dpad_left:
      return "Left";
    case provider::Button::dpad_right:
      return "Right";
    case provider::Button::left_stick:
      return "LS";
    case provider::Button::right_stick:
      return "RS";
    case provider::Button::sl:
      return "SL";
    case provider::Button::sr:
      return "SR";
  }
  return "?";
}

auto HintBar::hint(const composition::CommandHint& command, bool glyphs)
    -> composition::Element {
  composition::Element mark =
      glyphs && !glyph(command.button).empty()
          ? composition::Element{Text(std::string{glyph(command.button)})
                                     .font(theme::Font::headline)}
          : Text(std::string{name(command.button)})
                .font(theme::Font::footnote)
                .padding(theme::Insets::symmetric(theme::space::sm,
                                                  theme::space::xxs))
                .foreground_style(theme::ColourRole::background)
                .background(theme::ColourRole::text_secondary,
                            theme::Capsule());
  return HStack(
      HStackOptions{.alignment = VerticalAlignment::center,
                    .spacing = theme::space::xs},
      std::move(mark),
      Text(text::LocalizedStringKey{command.label}).font(theme::Font::callout));
}

auto HintBar::body(composition::Context& context) const
    -> composition::Element {
  const std::vector<composition::CommandHint>& hints =
      context.environment(Environment::command_hints);
  const bool glyphs = context.environment(Environment::controller_glyphs);
  const bool hidden =
      context.environment(Environment::persistent_system_overlays) ==
      composition::Visibility::hidden;
  std::vector<composition::Element> items;
  items.reserve(hidden ? 0 : hints.size());
  for (const composition::CommandHint& command : hints) {
    if (hidden) {
      break;
    }
    items.push_back(hint(command, glyphs));
  }
  return HStack(HStackOptions{.alignment = VerticalAlignment::center,
                              .spacing = theme::space::lg},
                std::move(items));
}

}
