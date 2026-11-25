#include <bench_screen.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/space.hpp>

#include <gallery_page.hpp>
#include <gallery_screen.hpp>

#include <format>
#include <ranges>
#include <string>
#include <utility>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;

const composition::StateKey<std::vector<int>> BenchScreen::path{"bench-path",
                                                                {}};

auto BenchScreen::body(composition::Context& context) const -> Element {
  switch (mode_) {
    case Mode::fling:
      return fling();
    case Mode::navigation: {
      const auto levels = context.state(path);
      remote_->path = levels;
      return navigation(levels);
    }
    case Mode::sheet: {
      const auto shown = context.state(presented);
      remote_->sheet = shown;
      return sheet(shown);
    }
    case Mode::sheet_glass: {
      const auto shown = context.state(presented);
      remote_->sheet = shown;
      return sheet_glass(shown, photo_);
    }
  }
  return fling();
}

auto BenchScreen::fling() -> Element {
  return List(Section(header(Text("Ten thousand rows")))(ForEach(
      std::views::iota(0, rows), [](int row) { return row; },
      [](int row) {
        return LabeledContent(std::format("Row {}", row),
                              std::format("#{}", row));
      })));
}

auto BenchScreen::navigation(composition::StateHandle<std::vector<int>> levels)
    -> Element {
  return NavigationStack(levels, [] {
    return VStack(spacing(theme::space::lg),
                  frame({.max_width = infinity, .max_height = infinity}))(
               Text(font::title2)("Root"), Text("Pushed by the benchmark"))
        .navigation_title("Bench")
        .navigation_destination(for_type<int>, [](int level) {
          return VStack(spacing(theme::space::md),
                        frame({.max_width = infinity, .max_height = infinity}))(
                     Text(font::title2)(std::format("Level {}", level)),
                     Text("Popped by the benchmark"))
              .navigation_title(std::format("Level {}", level));
        });
  });
}

auto BenchScreen::sheet(composition::StateHandle<bool> shown) -> Element {
  return VStack(spacing(theme::space::lg),
                frame({.max_width = infinity, .max_height = infinity}))(
             Text(font::title2)("Sheet host"), Text("The sheet slides up"))
      .sheet(shown, [] {
        return VStack(spacing(theme::space::md), padding(theme::space::lg))(
            Text(font::title2)("Sheet"), Text("Dismissed by the benchmark"));
      });
}

auto BenchScreen::sheet_glass(composition::StateHandle<bool> shown,
                              const Photo& photo) -> Element {
  return Element{GalleryScreen{GalleryPage::glass, theme::Seed{}, std::nullopt,
                               std::nullopt, std::nullopt, nullptr, photo}}
      .sheet(shown, [shown] {
        return VStack(spacing(theme::space::md), padding(theme::space::lg))(
            Text(font::title2)("Sheet"), Text("Dismissed by the benchmark"),
            Button(action([shown] { shown.set(false); }))("Done"));
      });
}

}
