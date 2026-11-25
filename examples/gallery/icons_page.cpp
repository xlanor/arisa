#include <icons_page.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <array>
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

auto IconsPage::card(std::string_view title, Element content) -> Element {
  return VStack(alignment::leading, spacing(theme::space::md),
                padding(theme::space::lg),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()),
                shadow(theme::Elevation::low))(
      Text(font::headline)(std::string{title}), std::move(content));
}

auto IconsPage::catalogue() -> Element {
  std::vector<Element> tiles;
  tiles.reserve(SystemSymbols::names.size());
  for (const std::string_view symbol : SystemSymbols::names) {
    tiles.push_back(
        VStack(spacing(theme::space::xxs), frame({.max_width = infinity}))(
            Image(font::title2)(system_name{std::string{symbol}}),
            Text(font::caption2, foreground_style(ColourRole::text_secondary))(
                std::string{symbol})));
  }
  return LazyVGrid(columns(8, theme::space::md), spacing(theme::space::md),
                   frame({.max_width = infinity}))(std::move(tiles));
}

auto IconsPage::variants() -> Element {
  constexpr std::array filled{"favorite", "star",          "home",
                              "bookmark", "notifications", "settings"};
  std::vector<Element> outlined_row;
  std::vector<Element> filled_row;
  for (const char* symbol : filled) {
    outlined_row.push_back(Element{Image(font::title2)(system_name{symbol})});
    filled_row.push_back(
        Element{Image(font::title2)(system_name{symbol})}.symbol_variant(
            SymbolVariants::fill));
  }
  constexpr std::array shaped{"add", "check", "remove"};
  std::vector<Element> circled;
  for (const char* symbol : shaped) {
    circled.push_back(
        Element{Image(font::title2)(system_name{symbol})}.symbol_variant(
            SymbolVariants::circle));
    circled.push_back(
        Element{Image(font::title2)(system_name{symbol})}.symbol_variant(
            SymbolVariants::circle.filled()));
  }
  constexpr std::array slashed{"visibility", "wifi", "notifications"};
  std::vector<Element> slashes;
  for (const char* symbol : slashed) {
    slashes.push_back(
        Element{Image(font::title2)(system_name{symbol})}.symbol_variant(
            SymbolVariants::slash));
  }
  return VStack(alignment::leading, spacing(theme::space::sm))(
      HStack(spacing(theme::space::md))(std::move(outlined_row)),
      HStack(spacing(theme::space::md))(std::move(filled_row)),
      HStack(spacing(theme::space::md))(std::move(circled)),
      HStack(spacing(theme::space::md))(std::move(slashes)));
}

auto IconsPage::scales() -> Element {
  constexpr std::array steps{std::pair{ImageScale::small, "small"},
                             std::pair{ImageScale::medium, "medium"},
                             std::pair{ImageScale::large, "large"}};
  std::vector<Element> shown;
  for (const auto& [scale, label] : steps) {
    shown.push_back(VStack(spacing(theme::space::xs))(
        Element{Image(font::body)(system_name{"star"})}.image_scale(scale),
        Text(font::caption2,
             foreground_style(ColourRole::text_secondary))(label)));
  }
  return HStack(alignment::bottom, spacing(theme::space::xl))(std::move(shown));
}

auto IconsPage::rendering() -> Element {
  return HStack(alignment::center, spacing(theme::space::lg))(
      Image(frame({.width = 48.0F, .height = 48.0F}))(
          "arisa/images/badge_48.png"),
      Image(rendering_mode(components::Image::TemplateRenderingMode::template_),
            foreground_style(ColourRole::accent),
            frame({.width = 48.0F, .height = 48.0F}))(
          "arisa/images/badge_48.png"),
      Image(rendering_mode(components::Image::TemplateRenderingMode::template_),
            foreground_style(ColourRole::destructive),
            frame({.width = 48.0F, .height = 48.0F}))(
          "arisa/images/badge_48.png"),
      Image(resizable, frame({.width = 48.0F, .height = 48.0F}))(
          "arisa/images/pixel_8.png"),
      Image(resizable, interpolation(components::Image::Interpolation::none),
            frame({.width = 48.0F, .height = 48.0F}))(
          "arisa/images/pixel_8.png"));
}

auto IconsPage::body(composition::Context&) const -> Element {
  constexpr std::array fonts{
      std::pair{theme::Font::caption, "caption"},
      std::pair{theme::Font::body, "body"},
      std::pair{theme::Font::title2, "title2"},
      std::pair{theme::Font::large_title, "large_title"}};
  std::vector<Element> sizes;
  for (const auto& [shown, label] : fonts) {
    sizes.push_back(VStack(spacing(theme::space::xs))(
        Image(font::of(shown))(system_name{"star"}),
        Text(font::caption2,
             foreground_style(ColourRole::text_secondary))(label)));
  }
  constexpr std::array tints{ColourRole::accent, ColourRole::success,
                             ColourRole::warning, ColourRole::destructive,
                             ColourRole::text_secondary};
  std::vector<Element> colours;
  for (const ColourRole tint : tints) {
    colours.push_back(
        Image(font::title2, foreground_style(tint))(system_name{"favorite"}));
  }
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    content_margins(theme::space::sm))(VStack(
      alignment::leading, spacing(theme::space::lg),
      frame({.max_width = infinity,
             .alignment = composition::Alignment::leading}))(
      HStack(alignment::top, spacing(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::top}))(
          card("Sizes follow the font",
               HStack(alignment::bottom,
                      spacing(theme::space::xl))(std::move(sizes))),
          card("Colours follow foreground_style",
               HStack(spacing(theme::space::lg))(std::move(colours))),
          card("Icons in controls",
               VStack(alignment::leading, spacing(theme::space::sm))(
                   Label("Downloads", Element{Image(system_name{"download"})}),
                   Button(action([] {}))(Element{Label(
                       "Settings", Element{Image(system_name{"settings"})})}),
                   HStack(spacing(theme::space::sm))(
                       Image(system_name{"wifi"}),
                       Image(system_name{"battery_5_bar"}),
                       Image(system_name{"bluetooth"}))))),
      HStack(alignment::top, spacing(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::top}))(
          card("Variants", variants()), card("Image scale", scales()),
          card("Rendering mode", rendering())),
      card("Catalogue", catalogue())));
}

}
