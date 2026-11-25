#include <gallery_page.hpp>

#include <algorithm>
#include <cstddef>

namespace arisa::examples {

auto GalleryPages::name(GalleryPage page) noexcept -> std::string_view {
  switch (page) {
    case GalleryPage::typography:
      return "typography";
    case GalleryPage::multilingual:
      return "multilingual";
    case GalleryPage::cards:
      return "cards";
    case GalleryPage::layout:
      return "layout";
    case GalleryPage::themes:
      return "themes";
    case GalleryPage::counter:
      return "counter";
    case GalleryPage::motion:
      return "motion";
    case GalleryPage::effects:
      return "effects";
    case GalleryPage::focus:
      return "focus";
    case GalleryPage::scroll:
      return "scroll";
    case GalleryPage::navigation:
      return "navigation";
    case GalleryPage::presentations:
      return "presentations";
    case GalleryPage::controls:
      return "controls";
    case GalleryPage::lists:
      return "lists";
    case GalleryPage::icons:
      return "icons";
    case GalleryPage::colours:
      return "colours";
    case GalleryPage::fidelity:
      return "fidelity";
    case GalleryPage::theming:
      return "theming";
    case GalleryPage::settings:
      return "settings";
    case GalleryPage::paths:
      return "paths";
    case GalleryPage::feed:
      return "feed";
    case GalleryPage::launch:
      return "launch";
    case GalleryPage::materials:
      return "materials";
    case GalleryPage::glass:
      return "glass";
    case GalleryPage::feedback:
      return "feedback";
    case GalleryPage::panels:
      return "panels";
    case GalleryPage::crashes:
      return "crashes";
  }
  return "typography";
}

auto GalleryPages::title(GalleryPage page) noexcept -> std::string_view {
  switch (page) {
    case GalleryPage::typography:
      return "Typography";
    case GalleryPage::multilingual:
      return "Languages";
    case GalleryPage::cards:
      return "Cards";
    case GalleryPage::layout:
      return "Layout";
    case GalleryPage::themes:
      return "Themes";
    case GalleryPage::counter:
      return "Counter";
    case GalleryPage::motion:
      return "Motion";
    case GalleryPage::effects:
      return "Effects";
    case GalleryPage::focus:
      return "Focus";
    case GalleryPage::scroll:
      return "Scroll";
    case GalleryPage::navigation:
      return "Navigation";
    case GalleryPage::presentations:
      return "Presentations";
    case GalleryPage::controls:
      return "Controls";
    case GalleryPage::lists:
      return "Lists";
    case GalleryPage::icons:
      return "Icons";
    case GalleryPage::colours:
      return "Colours";
    case GalleryPage::fidelity:
      return "Fidelity";
    case GalleryPage::theming:
      return "Theming";
    case GalleryPage::settings:
      return "Settings";
    case GalleryPage::paths:
      return "Paths";
    case GalleryPage::feed:
      return "Feed";
    case GalleryPage::launch:
      return "Launch";
    case GalleryPage::materials:
      return "Materials";
    case GalleryPage::glass:
      return "Glass";
    case GalleryPage::feedback:
      return "Feedback";
    case GalleryPage::panels:
      return "Panels";
    case GalleryPage::crashes:
      return "Crashes";
  }
  return "Typography";
}

auto GalleryPages::symbol(GalleryPage page) noexcept -> std::string_view {
  switch (page) {
    case GalleryPage::typography:
      return "text_fields";
    case GalleryPage::multilingual:
      return "translate";
    case GalleryPage::cards:
      return "dashboard";
    case GalleryPage::layout:
      return "grid_view";
    case GalleryPage::themes:
      return "palette";
    case GalleryPage::counter:
      return "add_circle";
    case GalleryPage::motion:
      return "rocket_launch";
    case GalleryPage::effects:
      return "celebration";
    case GalleryPage::focus:
      return "sports_esports";
    case GalleryPage::scroll:
      return "swap_vert";
    case GalleryPage::navigation:
      return "explore";
    case GalleryPage::presentations:
      return "open_in_new";
    case GalleryPage::controls:
      return "tune";
    case GalleryPage::lists:
      return "list";
    case GalleryPage::icons:
      return "category";
    case GalleryPage::colours:
      return "light_mode";
    case GalleryPage::fidelity:
      return "science";
    case GalleryPage::theming:
      return "brightness_high";
    case GalleryPage::settings:
      return "settings";
    case GalleryPage::paths:
      return "edit";
    case GalleryPage::feed:
      return "article";
    case GalleryPage::launch:
      return "replay";
    case GalleryPage::materials:
      return "category";
    case GalleryPage::glass:
      return "brightness_high";
    case GalleryPage::feedback:
      return "stadia_controller";
    case GalleryPage::panels:
      return "dashboard";
    case GalleryPage::crashes:
      return "warning";
  }
  return "text_fields";
}

auto GalleryPages::parse(std::string_view name) noexcept
    -> std::optional<GalleryPage> {
  const auto found = std::ranges::find_if(all, [name](GalleryPage page) {
    return GalleryPages::name(page) == name;
  });
  if (found == all.end()) {
    return std::nullopt;
  }
  return *found;
}

auto GalleryPages::next(GalleryPage page, bool forward) noexcept
    -> GalleryPage {
  const auto index =
      static_cast<std::size_t>(std::ranges::find(all, page) - all.begin());
  const std::size_t step = forward ? 1 : all.size() - 1;
  return all[(index + step) % all.size()];
}

}
