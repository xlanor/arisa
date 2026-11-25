#include <search_page.hpp>

#include <arisa/components/grid_item.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>

#include <array>
#include <cctype>
#include <cstddef>
#include <format>
#include <string>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

const composition::StateKey<std::vector<SearchPage::Genre>>
    SearchPage::tokens_key{"search-tokens", std::vector<Genre>{}};
const composition::StateKey<std::vector<SearchPage::Genre>>
    SearchPage::offered_key{
        "search-offered",
        std::vector<Genre>{Genre::action, Genre::racing, Genre::puzzle,
                           Genre::role_playing, Genre::platformer}};

auto SearchPage::genre_name(Genre genre) -> std::string_view {
  switch (genre) {
    case Genre::action:
      return "Action";
    case Genre::racing:
      return "Racing";
    case Genre::puzzle:
      return "Puzzle";
    case Genre::role_playing:
      return "RPG";
    case Genre::platformer:
      return "Platformer";
  }
  return "Action";
}

auto SearchPage::fold(std::string_view text) -> std::string {
  std::string folded;
  folded.reserve(text.size());
  for (const char letter : text) {
    folded.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
  }
  return folded;
}

auto SearchPage::titles() -> const std::vector<Title>& {
  static const std::vector<Title> made = [] {
    constexpr std::array first{"Star",  "Neon",   "Iron",  "Crystal", "Shadow",
                               "Turbo", "Pixel",  "Lunar", "Ember",   "Frost",
                               "Storm", "Velvet", "Quiet", "Golden",  "Echo"};
    constexpr std::array second{"Racer",   "Quest",   "Garden", "Tactics",
                                "Drift",   "Legend",  "Forge",  "Harbor",
                                "Circuit", "Odyssey", "Puzzle", "Rally",
                                "Kingdom", "Runner",  "Saga",   "Arena"};
    std::vector<Title> all;
    all.reserve(title_count);
    for (int index = 0; index < title_count; ++index) {
      const auto at = static_cast<std::size_t>(index);
      std::string name = std::format(
          "{} {} {}", first[at % first.size()],
          second[(at / first.size()) % second.size()],
          (index / static_cast<int>(first.size() * second.size())) + 1);
      std::string folded = fold(name);
      all.push_back(Title{.index = index,
                          .name = std::move(name),
                          .folded = std::move(folded),
                          .genre = static_cast<Genre>(at % 5),
                          .installed = index % 3 != 0,
                          .archived = index % 7 == 0});
    }
    return all;
  }();
  return made;
}

auto SearchPage::matches(const std::string& query,
                         const std::vector<Genre>& genres, Shelf shelf)
    -> std::vector<int> {
  const std::string wanted = fold(query);
  std::vector<int> found;
  for (const Title& title : titles()) {
    if (shelf == Shelf::installed && !title.installed) {
      continue;
    }
    if (shelf == Shelf::archived && !title.archived) {
      continue;
    }
    if (!genres.empty() &&
        std::ranges::find(genres, title.genre) == genres.end()) {
      continue;
    }
    if (!wanted.empty() && !title.folded.contains(wanted)) {
      continue;
    }
    found.push_back(title.index);
  }
  return found;
}

auto SearchPage::cover(const Title& title) -> Element {
  constexpr std::array shades{ColourRole::accent, ColourRole::success,
                              ColourRole::warning, ColourRole::destructive,
                              ColourRole::primary};
  const ColourRole shade =
      shades[static_cast<std::size_t>(title.genre) % shades.size()];
  return VStack(alignment::leading, spacing(theme::space::xs),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                focusable())(
      VStack(frame({.max_width = infinity}), aspect_ratio(0.75F),
             background(shade,
                        RoundedRectangle(theme::radius::card,
                                         RoundedCornerStyle::continuous)))(),
      Text(font::footnote, line_limit(1))(title.name),
      Text(font::caption, foreground_style(ColourRole::text_secondary),
           line_limit(1))(std::string{genre_name(title.genre)}));
}

auto SearchPage::results(const std::vector<int>& found,
                         const std::string& query) -> Element {
  if (found.empty()) {
    return Element{components::ContentUnavailableView::search(query)};
  }
  const std::vector<GridItem> tracks{
      GridItem{GridItem::Size::adaptive(cover_minimum), theme::space::md}};
  return ScrollView(id(results_view),
                    frame({.max_width = infinity, .max_height = infinity}),
                    content_margins(theme::space::sm))(
      LazyVGrid(columns(tracks), spacing(theme::space::md),
                estimated_extent(220.0F), overscan(120.0F))(ForEach(
          found, [](int index) { return index; },
          [](int index) {
            return cover(titles()[static_cast<std::size_t>(index)]);
          })));
}

auto SearchPage::suggestions(const std::string& query) -> Element {
  if (!query.empty()) {
    return Element{components::Group()};
  }
  constexpr std::array recent{"Star", "Drift", "Kingdom"};
  std::vector<Element> rows;
  for (const char* word : recent) {
    rows.push_back(
        Element{components::Label(std::string{word},
                                  Element{Image(system_name{"history"})})}
            .search_completion(std::string{word}));
  }
  return Element{components::HStack(
      components::HStackOptions{.spacing = theme::space::md}, std::move(rows))};
}

auto SearchPage::Summary::body(composition::Context& context) const -> Element {
  const bool searching =
      context.environment(components::Environment::is_searching);
  const composition::DismissSearchAction dismiss =
      context.environment(components::Environment::dismiss_search);
  if (!searching) {
    return Text(font::headline)(std::format("{} titles", count_));
  }
  return HStack(spacing(theme::space::md))(
      Text(font::headline, foreground_style(ColourRole::accent))(
          std::format("{} results", count_)),
      Button(id(done_button), action([dismiss] { dismiss(); }))("Done"));
}

auto SearchPage::body(composition::Context& context) const -> Element {
  const auto shown = shown_;
  const auto query = context.state(query_key);
  const auto shelf = context.state(shelf_key);
  const auto chosen = context.state(tokens_key);
  const auto offered = context.state(offered_key);
  const auto submits = context.state(submits_key);
  const std::vector<int> found =
      matches(query.get(), chosen.get(), shelf.get());
  Element listing =
      VStack(alignment::leading, spacing(theme::space::sm),
             frame({.max_width = infinity,
                    .max_height = infinity,
                    .alignment = composition::Alignment::top_leading}))(
          Element{Summary{found.size()}}, results(found, query.get()));
  return VStack(
      alignment::leading, spacing(theme::space::md), padding(theme::space::lg),
      frame({.max_width = infinity,
             .max_height = infinity,
             .alignment = composition::Alignment::top_leading}),
      background(ColourRole::system_background, theme::Shape::rectangle()))(
      Button(id(close_button), button_style(ButtonStyle::bordered()),
             action([shown] { shown.set(false); }))("Close"),
      std::move(listing)
          .searchable(
              query, chosen, offered,
              [](Genre genre) {
                return Element{
                    components::Text(std::string{genre_name(genre)})};
              },
              SearchFieldPlacement::automatic, std::string{"Titles"})
          .search_scopes(shelf,
                         [] {
                           return Element{
                               Group(Text(tag(Shelf::all))("All"),
                                     Text(tag(Shelf::installed))("Installed"),
                                     Text(tag(Shelf::archived))("Archived"))};
                         })
          .search_suggestions(
              [text = query.get()] { return suggestions(text); })
          .on_submit(SubmitTriggers::search,
                     [submits] { submits.set(submits.get() + 1); }));
}

}
