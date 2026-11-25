#include <library_page.hpp>

#include <arisa/components/grid_item.hpp>
#include <arisa/components/grid_section.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/pinned_scrollable_views.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <ranges>
#include <string>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

auto LibraryPage::cover(int index, int load) -> Element {
  constexpr std::array shades{ColourRole::accent, ColourRole::success,
                              ColourRole::warning, ColourRole::destructive,
                              ColourRole::primary};
  const ColourRole shade =
      shades[static_cast<std::size_t>(index + load) % shades.size()];
  return VStack(alignment::leading, spacing(theme::space::xs),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                focusable())(
      VStack(frame({.max_width = infinity}), aspect_ratio(0.75F),
             background(shade,
                        RoundedRectangle(theme::radius::card,
                                         RoundedCornerStyle::continuous)))(),
      Text(font::footnote, line_limit(1))(std::format("{}", index + 1)));
}

auto LibraryPage::grid(int load, bool sectioned) -> Element {
  const std::vector<GridItem> tracks{
      GridItem{GridItem::Size::adaptive(cover_minimum), theme::space::md}};
  if (!sectioned) {
    return LazyVGrid(columns(tracks), spacing(theme::space::md),
                     estimated_extent(240.0F), overscan(120.0F))(ForEach(
        std::views::iota(0, cover_count),
        [](int index) { return cover_key(index); },
        [load](int index) { return cover(index, load); }));
  }
  constexpr int per = (cover_count + section_count - 1) / section_count;
  std::vector<components::GridSection> sections;
  sections.reserve(section_count);
  for (int section = 0; section < section_count; ++section) {
    const int first = section * per;
    const int last = std::min(cover_count, first + per);
    sections.emplace_back(
        Element{Text(font::headline,
                     padding(theme::Insets::symmetric(theme::space::sm,
                                                      theme::space::xs)),
                     frame({.max_width = infinity,
                            .alignment = composition::Alignment::leading}),
                     background(ColourRole::system_background,
                                theme::Shape::rectangle()))(
            std::string(1, static_cast<char>('A' + section)))},
        components::ForEach(
            std::views::iota(first, last),
            [](int index) { return cover_key(index); },
            [load](int index) { return cover(index, load); }));
  }
  return components::LazyVGrid(
      components::LazyVGridOptions{
          .columns = tracks,
          .spacing = theme::space::md,
          .overscan = 120.0F,
          .estimated_extent = 240.0F,
          .pinned_views = components::PinnedScrollableViews::section_headers},
      std::move(sections));
}

auto LibraryPage::body(composition::Context& context) const -> Element {
  const auto loads = context.state(loads_key);
  const auto sectioned = context.state(sectioned_key);
  const int load = loads.get();
  const auto shown = shown_;
  return VStack(
      alignment::leading, spacing(theme::space::md), padding(theme::space::lg),
      frame({.max_width = infinity,
             .max_height = infinity,
             .alignment = composition::Alignment::leading}),
      background(ColourRole::system_background, theme::Shape::rectangle()))(
      HStack(spacing(theme::space::md))(
          Button(id(close_button),
                 action([shown] { shown.set(false); }))("Close"),
          Button(id(reload_button),
                 action([loads] { loads.set(loads.get() + 1); }))("Reload"),
          Button(id(sections_button), action([sectioned] {
                   sectioned.set(!sectioned.get());
                 }))("Sections")),
      ScrollView(
          id(library_view),
          frame({.max_width = infinity, .max_height = infinity}),
          content_margins(theme::space::sm))(grid(load, sectioned.get())));
}

}
