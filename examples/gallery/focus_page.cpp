#include <focus_page.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <format>
#include <optional>
#include <ranges>
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

auto FocusPage::heading(std::string_view title) -> Element {
  return Text(font::headline)(std::string{title});
}

auto FocusPage::body(composition::Context& context) const -> Element {
  const auto focus = context.state(cell);
  const auto loads = context.state(generation);
  const auto gone = context.state(dismissed);
  const auto last = context.state(pressed);
  Element content = HStack(
      alignment::top, spacing(theme::space::xl), wrap,
      frame({.max_width = infinity, .alignment = composition::Alignment::top}),
      on_exit_command("Reset",
                      [focus, gone] {
                        gone.set(false);
                        focus.set(default_cell);
                      }))(
      VStack(alignment::leading, spacing(theme::space::md),
             frame({.width = 380.0F,
                    .alignment = composition::Alignment::leading}))(
          HStack(alignment::top, spacing(theme::space::xl))(
              section("Section A", {"Alpha", "Bravo", "Charlie"}, last),
              section("Section B", {"Delta", "Echo", "Foxtrot"}, last))),
      cells(focus), reloading(loads, gone, last));
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}))(
      std::move(content));
}

auto FocusPage::section(std::string_view title,
                        std::array<std::string_view, 3> labels, Pressed last)
    -> Element {
  std::vector<Element> items;
  items.push_back(heading(title));
  for (const std::string_view label : labels) {
    items.push_back(Button(action([last, label] { last.set(label); }),
                           frame({.width = 140.0F}))(std::string{label}));
  }
  return VStack(
      alignment::leading, spacing(theme::space::md), padding(theme::space::lg),
      background(ColourRole::surface_raised,
                 RoundedRectangle(theme::radius::card,
                                  RoundedCornerStyle::continuous)),
      border(ColourRole::separator, theme::Stroke::hairline()),
      shadow(theme::Elevation::low), focus_section())(std::move(items));
}

auto FocusPage::cells(composition::FocusBinding<int> focus) -> Element {
  std::vector<Element> grid;
  for (int index = 1; index <= cell_count; ++index) {
    grid.push_back(Button(action([focus, index] { focus.set(index); }),
                          frame({.width = 64.0F, .height = 48.0F}),
                          focused(focus, index))(std::format("{}", index)));
  }
  const std::optional<int> current = focus.get();
  return VStack(
      alignment::leading, spacing(theme::space::md),
      frame({.width = 260.0F, .alignment = composition::Alignment::leading}),
      sensory_feedback(SensoryFeedback::selection, current))(
      heading("FocusState"),
      LazyVGrid(columns(3, theme::space::sm), spacing(theme::space::sm),
                frame({.width = 216.0F}),
                default_focus(focus, default_cell))(std::move(grid)),
      Button(action([focus] { focus.set(2); }),
             id(jump_button))("Focus cell 2"),
      Text(font::footnote, foreground_style(ColourRole::text_secondary))(
          current.has_value() ? std::format("focused cell: {}", *current)
                              : std::string{"focused cell: none"}));
}

auto FocusPage::reloading(composition::StateHandle<int> loads,
                          composition::StateHandle<bool> gone, Pressed last)
    -> Element {
  const int load = loads.get();
  std::vector<Element> column;
  column.push_back(heading("Reload and depart"));
  column.push_back(VStack(
      alignment::leading, spacing(theme::space::sm),
      id(std::format("focus-list-{}", load)),
      on_play_pause_command("Reload", [loads] { loads.set(loads.get() + 1); }))(
      ForEach(
          std::views::iota(1, row_count + 1),
          [](int row) { return std::format("row-{}", row); },
          [load, last](int row) {
            return Button(action([last] { last.set("a row"); }),
                          frame({.width = 200.0F}))(
                std::format("Row {}  load {}", row, load));
          })));
  column.push_back(Button(action([loads] { loads.set(loads.get() + 1); }),
                          id(reload_button))("Reload list"));
  if (!gone.get()) {
    column.push_back(Button(action([gone] { gone.set(true); }),
                            id(dismiss_button))("Dismiss me"));
  }
  return VStack(
      alignment::leading, spacing(theme::space::md),
      frame({.width = 280.0F, .alignment = composition::Alignment::leading}))(
      std::move(column));
}

}
