#include <colours_page.hpp>

#include <arisa/appearance/theme_vocabulary.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/oklch.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <array>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

auto ColoursPage::card_key(std::string_view role) -> std::string {
  return std::format("colours.{}", role);
}

auto ColoursPage::chip_key(std::string_view role, std::string_view variant)
    -> std::string {
  return std::format("colours.{}.{}", role, variant);
}

auto ColoursPage::chip(const Swatch& entry, const Variant& variant,
                       theme::Colour colour) -> Element {
  const theme::Colour mat = theme::Oklch::from(colour).lightness() > mat_split
                                ? theme::Colour::hex(0x000000)
                                : theme::Colour::hex(0xFFFFFF);
  return VStack(alignment::leading, spacing(theme::space::xxs))(
      VStack(frame({.width = chip_width, .height = chip_height}),
             padding(mat_inset), background(mat, RoundedRectangle(6.0F)))(
          VStack(frame({.max_width = infinity, .max_height = infinity}),
                 background(colour, RoundedRectangle(4.0F)),
                 id(chip_key(entry.name, variant.name)))()),
      Text(font::caption2,
           foreground_style(ColourRole::secondary_label))(std::format(
          "{}\n{}", variant.name, appearance::ThemeVocabulary::hex(colour))));
}

auto ColoursPage::shows(const theme::AppearanceVariants& resolved,
                        const Variant& variant) -> bool {
  return variant.base == nullptr ||
         resolved.*variant.value != resolved.*variant.base;
}

auto ColoursPage::card(const Swatch& entry, const theme::Theme& theme)
    -> Element {
  const theme::AppearanceVariants& resolved = theme.variants(entry.role);
  std::vector<Element> chips;
  chips.reserve(variants.size());
  for (const Variant& variant : variants) {
    if (shows(resolved, variant)) {
      chips.push_back(chip(entry, variant, resolved.*variant.value));
    }
  }
  return VStack(alignment::leading, spacing(theme::space::sm),
                padding(theme::space::md),
                background(ColourRole::secondary_system_grouped_background,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()),
                focusable(), id(card_key(entry.name)))(
      Text(font::headline)(std::string{entry.name}),
      HStack(alignment::top, spacing(theme::space::sm),
             wrap)(std::move(chips)));
}

auto ColoursPage::section(std::string_view title,
                          std::span<const Swatch> swatches,
                          const theme::Theme& theme) -> Element {
  std::vector<Element> cards;
  cards.reserve(swatches.size());
  for (const Swatch& entry : swatches) {
    cards.push_back(card(entry, theme));
  }
  return VStack(alignment::leading, spacing(theme::space::md),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      Text(font::title3)(std::string{title}),
      HStack(
          alignment::top, spacing(theme::space::md), wrap,
          frame({.max_width = infinity,
                 .alignment = composition::Alignment::top}))(std::move(cards)));
}

auto ColoursPage::groups() -> std::span<const std::span<const Swatch>> {
  static constexpr std::array<std::span<const Swatch>, 4> all{
      adaptive, backgrounds, tints, macos};
  return all;
}

auto ColoursPage::body(composition::Context& context) const -> Element {
  const theme::Theme& theme =
      context.environment(components::Environment::theme);
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    content_margins(theme::space::sm))(
      VStack(alignment::leading, spacing(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::leading}))(
          section("Adaptive elements", adaptive, theme),
          section("Backgrounds and fills", backgrounds, theme),
          section("Structural tints", tints, theme),
          section("macOS roles", macos, theme)));
}

}
