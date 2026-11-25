#include <navigation_page.hpp>

#include <chrome_page.hpp>
#include <data_flow_page.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <array>
#include <format>
#include <string>
#include <utility>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

const composition::StateKey<std::vector<NavigationPage::Chapter>>
    NavigationPage::path{"navigation-path", {}};

auto NavigationPage::chapter(int level, std::string_view name) -> Chapter {
  return Chapter{.level = level, .name = std::string{name}};
}

auto NavigationPage::body(composition::Context& context) const -> Element {
  const auto chapters = context.state(path);
  return NavigationStack(chapters, [chapters] {
    return library(chapters).navigation_title("Library").navigation_destination(
        for_type<Chapter>, [chapters](const Chapter& chapter) {
          return Element{Detail{chapter, chapters}}.navigation_title(
              std::format("Level {}: {}", chapter.level, chapter.name));
        });
  });
}

auto NavigationPage::library(
    composition::StateHandle<std::vector<Chapter>> chapters) -> Element {
  constexpr std::array names{"Oceans", "Mountains", "Deserts"};
  std::vector<Element> links;
  links.push_back(Text(font::headline)("Destinations"));
  for (const char* name : names) {
    links.push_back(Element{NavigationLink(chapter(1, name), std::string{name})}
                        .frame({.width = 220.0F})
                        .id(std::string{name}));
  }
  return ScrollView(
      frame({.max_width = infinity, .max_height = infinity}))(VStack(
      alignment::leading, spacing(theme::space::lg),
      frame({.max_width = infinity,
             .alignment = composition::Alignment::leading}))(
      HStack(alignment::top, spacing(theme::space::xl))(
          VStack(alignment::leading, spacing(theme::space::md),
                 padding(theme::space::lg), id(root_list),
                 background(ColourRole::surface_raised,
                            RoundedRectangle(theme::radius::card,
                                             RoundedCornerStyle::continuous)),
                 border(ColourRole::separator, theme::Stroke::hairline()),
                 shadow(theme::Elevation::low),
                 focus_section())(std::move(links)),
          VStack(alignment::leading, spacing(theme::space::md),
                 frame({.width = 360.0F,
                        .alignment = composition::Alignment::leading}))(
              Button(action([chapters] {
                       chapters.set({chapter(1, "Oceans"), chapter(2, "Oceans"),
                                     chapter(3, "Oceans")});
                     }),
                     id(jump_button))("Jump three levels"),
              DataFlowPage::Opener{}, ChromePage::Opener{}))));
}

auto NavigationPage::Detail::body(composition::Context& context) const
    -> Element {
  const auto count = context.state(taps);
  const auto chapters = chapters_;
  std::vector<Element> actions;
  actions.push_back(
      Button(action([count] { count.set(count.get() + 1); }),
             id(tap_button))(std::format("Tapped {} times", count.get())));
  if (chapter_.level < deepest) {
    actions.push_back(Element{
        NavigationLink(chapter(chapter_.level + 1, chapter_.name),
                       std::format("Go to level {}", chapter_.level + 1))}
                          .id(deeper_link));
  }
  actions.push_back(Button(action([chapters] { chapters.set({}); }),
                           id(root_button))("Pop to root"));
  return VStack(alignment::leading, spacing(theme::space::lg),
                padding(theme::Edge::top, theme::space::md))(
      Text(font::body)(std::format("{} at depth {} of {}.", chapter_.name,
                                   chapter_.level, deepest)),
      HStack(spacing(theme::space::md))(std::move(actions)));
}

}
