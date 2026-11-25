#include <layout_scroll_page.hpp>

#include <arisa/components/arrangement_view.hpp>
#include <arisa/components/safe_area_inset.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/safe_area_regions.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>

#include <array>
#include <format>
#include <ranges>
#include <string>

namespace arisa::examples {

using namespace markup;
using components::ArrangementView;
using components::ArrangementViewStyle;
using composition::ContentMarginPlacement;
using composition::Element;
using composition::infinity;
using composition::SafeAreaRegions;
using composition::ScrollDismissesKeyboardMode;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

const composition::StateKey<std::string> LayoutScrollPage::immediate_text{
    "layout-scroll-immediate-text", std::string{}};
const composition::StateKey<std::string> LayoutScrollPage::never_text{
    "layout-scroll-never-text", std::string{}};

auto LayoutScrollPage::lazy_row_key(int index) -> std::string {
  return std::format("layout-scroll-lazy-row-{}", index);
}

auto LayoutScrollPage::chip_key(int index) -> std::string {
  return std::format("{}{}", chip_prefix, index);
}

auto LayoutScrollPage::avatar_key(int index) -> std::string {
  return std::format("{}{}", avatar_prefix, index);
}

auto LayoutScrollPage::card_key(int index) -> std::string {
  return std::format("layout-scroll-card-{}", index);
}

auto LayoutScrollPage::page_key(int index) -> std::string {
  return std::format("layout-scroll-page-{}", index);
}

auto LayoutScrollPage::margin_row_key(std::string_view view, int index)
    -> std::string {
  return std::format("{}-row-{}", view, index);
}

auto LayoutScrollPage::keyboard_row_key(std::string_view view, int index)
    -> std::string {
  return std::format("{}-row-{}", view, index);
}

auto LayoutScrollPage::arrangement_style(Style style) -> ArrangementViewStyle {
  switch (style) {
    case Style::split:
      return ArrangementViewStyle::split();
    case Style::overlay:
      return ArrangementViewStyle::overlay();
    case Style::automatic:
      break;
  }
  return ArrangementViewStyle::automatic();
}

auto LayoutScrollPage::ratio(Share share) noexcept -> float {
  switch (share) {
    case Share::third:
      return 1.0F / 3.0F;
    case Share::two_thirds:
      return 2.0F / 3.0F;
    case Share::half:
      break;
  }
  return 0.5F;
}

auto LayoutScrollPage::section(std::string_view title, Element content)
    -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm),
                padding(theme::space::md),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::top_leading}),
                background(ColourRole::surface,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)))(
      Text(font::headline)(std::string{title}), std::move(content));
}

auto LayoutScrollPage::caption(std::string_view text) -> Element {
  return Text(font::caption,
              foreground_style(ColourRole::text_secondary))(std::string{text});
}

auto LayoutScrollPage::lazy() -> Element {
  return ScrollView(id(lazy_view),
                    frame({.height = well_height, .max_width = infinity}))(
      components::LazyVStack(
          components::LazyVStackOptions{.spacing = 4.0F,
                                        .estimated_extent = 32.0F},
          Text(font::subheadline, id(lazy_header),
               padding(theme::Edge::vertical, theme::space::xxs))("Header"),
          ForEach(
              std::views::iota(0, lazy_rows),
              [](int index) { return lazy_row_key(index); },
              [](int index) {
                return Text(
                    font::body,
                    padding(theme::Edge::horizontal, theme::space::sm),
                    frame({.height = 32.0F,
                           .max_width = infinity,
                           .alignment = composition::Alignment::leading}),
                    background(
                        ColourRole::surface_raised,
                        RoundedRectangle(theme::radius::control,
                                         RoundedCornerStyle::continuous)),
                    focusable())(std::format("Row {}", index + 1));
              }),
          Text(font::subheadline, id(lazy_footer),
               padding(theme::Edge::vertical, theme::space::xxs))("Footer")));
}

auto LayoutScrollPage::region(std::string_view key, std::string_view label,
                              Element content) -> Element {
  const auto bar = [] {
    return VStack(
        frame({.height = region_inset, .max_width = infinity}),
        background(theme::ColourValue{ColourRole::label}.opacity(0.18F),
                   theme::Shape::rectangle()))();
  };
  return VStack(spacing(theme::space::xs))(
      VStack(spacing(0.0F))(std::move(content))
          .safe_area_inset(composition::VerticalEdge::top,
                           composition::HorizontalAlignment::center, 0.0F,
                           bar())
          .safe_area_inset(composition::VerticalEdge::bottom,
                           composition::HorizontalAlignment::center, 0.0F,
                           bar())
          .frame({.width = region_width, .height = region_height})
          .background(ColourRole::surface_raised,
                      RoundedRectangle(theme::radius::control,
                                       RoundedCornerStyle::continuous))
          .border(ColourRole::separator, theme::Stroke::hairline())
          .id(std::string{key}),
      caption(label));
}

auto LayoutScrollPage::safe_areas() -> Element {
  const auto fill = [](std::string_view key) {
    return VStack(frame({.max_width = infinity, .max_height = infinity}),
                  background(ColourRole::accent, theme::Shape::rectangle()),
                  id(std::string{key}))();
  };
  return HStack(alignment::top, spacing(theme::space::md))(
      region(ignored_region, "Ignores",
             fill(ignored_fill).ignores_safe_area(SafeAreaRegions::container)),
      region(respected_region, "Respects", fill(respected_fill)),
      region(aligned_region, "Aligned",
             VStack(spacing(0.0F))(
                 VStack(frame({.width = 48.0F, .height = aligned_chip}),
                        background(ColourRole::accent, theme::Capsule()),
                        id(std::string{aligned_fill}))()
                     .ignores_safe_area(SafeAreaRegions::container,
                                        theme::Edge::top,
                                        composition::Alignment::center),
                 Spacer())));
}

auto LayoutScrollPage::chip(int index, std::string_view label) -> Element {
  return Text(font::subheadline, foreground_style(ColourRole::on_accent),
              padding(theme::Edge::horizontal, theme::space::md),
              padding(theme::Edge::vertical, theme::space::xs),
              background(index == 1 ? ColourRole::success : ColourRole::accent,
                         theme::Capsule()),
              border(ColourRole::surface, theme::Stroke::hairline()))(
      std::string{label});
}

auto LayoutScrollPage::negative() -> Element {
  constexpr std::array colours{ColourRole::accent, ColourRole::success,
                               ColourRole::warning, ColourRole::destructive,
                               ColourRole::primary};
  return VStack(alignment::leading, spacing(theme::space::sm))(
      HStack(spacing(0.0F))(
          chip(0, "One").id(chip_key(0)),
          ZStack(chip(1, "Two").id(std::string{overlapped_chip}))
              .padding(chip_overlap)
              .z_index(1.0)
              .id(chip_key(1)),
          chip(2, "Three").id(chip_key(2))),
      caption("Padding"),
      HStack(spacing(avatar_spacing))(ForEach(
          std::views::iota(0, avatar_count),
          [](int index) { return avatar_key(index); },
          [colours](int index) {
            return VStack(frame({.width = avatar_size, .height = avatar_size}),
                          background(colours[static_cast<std::size_t>(index)],
                                     theme::Circle()),
                          border(ColourRole::surface, theme::Stroke{2.0F}))();
          })),
      caption("Spacing"));
}

auto LayoutScrollPage::pane(std::string_view label, ColourRole role)
    -> Element {
  return VStack(
      alignment::center, justify::center,
      frame({.max_width = infinity, .max_height = infinity}),
      background(role, RoundedRectangle(theme::radius::control,
                                        RoundedCornerStyle::continuous)))(
      Text(font::subheadline,
           foreground_style(ColourRole::on_accent))(std::string{label}));
}

auto LayoutScrollPage::arrangement(composition::StateHandle<Style> style,
                                   composition::StateHandle<Share> share)
    -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      Picker(picker_style(SegmentedPickerStyle{}),
             id(std::string{style_picker}))(
          "Style", style,
          [] {
            return Group(Text(tag(Style::automatic))("Automatic"),
                         Text(tag(Style::split))("Split"),
                         Text(tag(Style::overlay))("Overlay"));
          }),
      Picker(picker_style(SegmentedPickerStyle{}),
             id(std::string{share_picker}))(
          "Ratio", share,
          [] {
            return Group(Text(tag(Share::third))("Third"),
                         Text(tag(Share::half))("Half"),
                         Text(tag(Share::two_thirds))("Two thirds"));
          }),
      ArrangementView(
          pane("Primary", ColourRole::accent).id(std::string{primary_view}),
          pane("Secondary", ColourRole::success)
              .frame({.ideal_width = secondary_ideal,
                      .max_width = infinity,
                      .ideal_height = secondary_ideal * 0.25F,
                      .max_height = infinity})
              .split_arrangement_layout_ratio(ratio(share.get()))
              .id(std::string{secondary_view}))
          .arrangement_view_style(arrangement_style(style.get()))
          .frame({.height = well_height, .max_width = infinity})
          .id(std::string{arrangement_view}));
}

auto LayoutScrollPage::carousel() -> Element {
  return ScrollView(axes(Axes::horizontal), id(std::string{carousel_view}),
                    frame({.max_width = infinity}))(
             HStack(spacing(theme::space::md), scroll_target_layout())(ForEach(
                 std::views::iota(0, card_count),
                 [](int index) { return card_key(index); },
                 [](int index) {
                   return Text(
                       font::headline,
                       frame({.width = card_width, .height = strip_height}),
                       background(
                           ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                       border(ColourRole::separator, theme::Stroke::hairline()),
                       focusable())(std::format("Card {}", index + 1));
                 })))
      .scroll_target_behavior(ViewAlignedScrollTargetBehavior{
          ViewAlignedScrollTargetBehavior::LimitBehavior::always_by_one,
          UnitPoint::center});
}

auto LayoutScrollPage::paging() -> Element {
  constexpr std::array colours{ColourRole::accent, ColourRole::success,
                               ColourRole::warning, ColourRole::primary};
  return ScrollView(axes(Axes::horizontal), id(std::string{paging_view}),
                    frame({.max_width = infinity}))(
             HStack(spacing(0.0F))(ForEach(
                 std::views::iota(0, page_count),
                 [](int index) { return page_key(index); },
                 [colours](int index) {
                   return Text(font::headline,
                               foreground_style(ColourRole::on_accent))(
                              std::format("Page {}", index + 1))
                       .frame({.height = strip_height})
                       .container_relative_frame(Axes::horizontal)
                       .background(colours[static_cast<std::size_t>(index)],
                                   theme::Shape::rectangle())
                       .focusable();
                 })))
      .scroll_target_behavior(PagingScrollTargetBehavior{});
}

auto LayoutScrollPage::well(std::string_view key, std::string_view label,
                            ContentMarginPlacement placement) -> Element {
  return VStack(alignment::leading, spacing(theme::space::xs),
                frame({.max_width = infinity}))(
      caption(label),
      ScrollView(id(std::string{key}),
                 frame({.height = well_height, .max_width = infinity}),
                 scroll_indicators(ScrollIndicatorVisibility::visible),
                 background(ColourRole::secondary_system_fill,
                            RoundedRectangle(theme::radius::control,
                                             RoundedCornerStyle::continuous)))(
          VStack(spacing(theme::space::xxs),
                 frame({.max_width = infinity}))(ForEach(
              std::views::iota(0, margin_rows),
              [key](int index) { return margin_row_key(key, index); },
              [](int index) {
                return Text(
                    font::body,
                    frame({.height = 28.0F,
                           .max_width = infinity,
                           .alignment = composition::Alignment::leading}),
                    padding(theme::Edge::horizontal, theme::space::sm),
                    background(ColourRole::surface_raised,
                               theme::Shape::rectangle()),
                    focusable())(std::format("Row {}", index + 1));
              })))
          .content_margins(margin_inset, placement));
}

auto LayoutScrollPage::margins() -> Element {
  return HStack(alignment::top, spacing(theme::space::md),
                frame({.max_width = infinity}))(
      well(content_margin_view, "Content",
           ContentMarginPlacement::scroll_content),
      well(indicator_margin_view, "Indicators",
           ContentMarginPlacement::scroll_indicators));
}

auto LayoutScrollPage::typing(std::string_view key, std::string_view field,
                              std::string_view label,
                              composition::StateHandle<std::string> text,
                              ScrollDismissesKeyboardMode mode) -> Element {
  return VStack(alignment::leading, spacing(theme::space::xs),
                frame({.max_width = infinity}))(
      caption(label),
      ScrollView(id(std::string{key}),
                 frame({.height = well_height, .max_width = infinity}))(
          VStack(alignment::leading, spacing(theme::space::xxs),
                 frame({.max_width = infinity}))(
              TextField(id(std::string{field}))("Name", text),
              ForEach(
                  std::views::iota(0, keyboard_rows),
                  [key](int index) { return keyboard_row_key(key, index); },
                  [](int index) {
                    return Text(
                        font::body,
                        padding(theme::Edge::horizontal, theme::space::sm),
                        frame({.height = 28.0F,
                               .max_width = infinity,
                               .alignment = composition::Alignment::leading}),
                        background(
                            ColourRole::surface_raised,
                            RoundedRectangle(theme::radius::control,
                                             RoundedCornerStyle::continuous)),
                        focusable())(std::format("Row {}", index + 1));
                  })))
          .scroll_dismisses_keyboard(mode));
}

auto LayoutScrollPage::keyboard(composition::StateHandle<std::string> immediate,
                                composition::StateHandle<std::string> never)
    -> Element {
  return HStack(alignment::top, spacing(theme::space::md),
                frame({.max_width = infinity}))(
      typing(immediate_list, immediate_field, "Immediately", immediate,
             ScrollDismissesKeyboardMode::immediately),
      typing(never_list, never_field, "Never", never,
             ScrollDismissesKeyboardMode::never));
}

auto LayoutScrollPage::body(composition::Context& context) const -> Element {
  const auto shown = shown_;
  const auto style = context.state(style_key);
  const auto share = context.state(share_key);
  const auto immediate = context.state(immediate_text);
  const auto never = context.state(never_text);
  const auto row = [](Element first, Element second) {
    return HStack(alignment::top, spacing(theme::space::md),
                  frame({.max_width = infinity,
                         .alignment = composition::Alignment::leading}))(
        std::move(first), std::move(second));
  };
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    background(ColourRole::system_grouped_background,
                               theme::Shape::rectangle()))(
      VStack(alignment::leading, spacing(theme::space::md),
             padding(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::top_leading}))(
          Button(id(std::string{close_button}),
                 button_style(ButtonStyle::bordered()),
                 action([shown] { shown.set(false); }))("Close"),
          HStack(alignment::top, spacing(theme::space::md),
                 frame({.max_width = infinity,
                        .alignment = composition::Alignment::leading}))(
              section("Lazy stack", lazy()), section("Safe area", safe_areas()),
              section("Negative space", negative())),
          row(section("Arrangement", arrangement(style, share)),
              section("Carousel",
                      VStack(alignment::leading, spacing(theme::space::sm),
                             frame({.max_width = infinity}))(
                          carousel(), caption("Paging"), paging()))),
          row(section("Margins", margins()),
              section("Keyboard", keyboard(immediate, never)))));
}

auto LayoutScrollPage::Link::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return Button(id(std::string{button}), button_style(ButtonStyle::bordered()),
                action([shown] { shown.set(true); }),
                full_screen_cover(shown, [shown] {
                  return Element{LayoutScrollPage{shown}};
                }))("Layout and scroll");
}

}
