#include <arisa/composition/navigation_declarations.hpp>
#include <arisa/composition/toolbar_item.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace arisa::composition {

const PreferenceKey<std::optional<std::string>>
    NavigationDeclarations::title_key{
        "navigation-title", std::nullopt,
        [](std::optional<std::string>& value, std::optional<std::string> next) {
          if (!value.has_value()) {
            value = std::move(next);
          }
        }};

const PreferenceKey<NavigationDeclarations::Toolbar>
    NavigationDeclarations::toolbar_key{
        "navigation-toolbar",
        {},
        [](Toolbar& value, Toolbar next) {
          std::ranges::move(next, std::back_inserter(value));
        },
        [](const Toolbar& previous, const Toolbar& incoming) {
          if (previous.size() != incoming.size() ||
              !std::ranges::equal(
                  previous, incoming,
                  [](const std::shared_ptr<const ToolbarItem>& held,
                     const std::shared_ptr<const ToolbarItem>& next) {
                    return held->same_shape(*next);
                  })) {
            return false;
          }
          for (std::size_t index = 0; index < previous.size(); ++index) {
            if (previous[index]->content() != nullptr &&
                incoming[index]->content() != nullptr) {
              previous[index]->content()->replace(*incoming[index]->content());
            }
          }
          return true;
        }};

const PreferenceKey<NavigationDeclarations::Destinations>
    NavigationDeclarations::destinations_key{
        "navigation-destinations",
        {},
        [](Destinations& value, Destinations next) {
          std::ranges::move(next, std::back_inserter(value));
        }};

const PreferenceKey<std::optional<PresentationBackground>>
    NavigationDeclarations::background_key{
        "navigation-background", std::nullopt,
        [](std::optional<PresentationBackground>& value,
           std::optional<PresentationBackground> next) {
          if (!value.has_value()) {
            value = std::move(next);
          }
        }};

const PreferenceKey<NavigationChrome> NavigationDeclarations::chrome_key{
    "navigation-chrome", NavigationChrome{},
    [](NavigationChrome& value, NavigationChrome next) {
      value.override_with(next);
    }};

const PreferenceKey<std::optional<NavigationSplitViewColumnWidth>>
    NavigationDeclarations::column_width_key{
        "navigation-column-width", std::nullopt,
        [](std::optional<NavigationSplitViewColumnWidth>& value,
           std::optional<NavigationSplitViewColumnWidth> next) {
          if (!value.has_value()) {
            value = next;
          }
        }};

const EnvironmentKey<NavigationSplitViewStyle>
    NavigationDeclarations::split_view_style{
        "navigation_split_view_style", NavigationSplitViewStyle::automatic};

const EnvironmentKey<Visibility>
    NavigationDeclarations::link_indicator_visibility{
        "navigation_link_indicator_visibility", Visibility::automatic};

auto NavigationDeclarations::isolates(Identity preference) noexcept -> bool {
  return preference == title_key.identity() ||
         preference == toolbar_key.identity() ||
         preference == destinations_key.identity() ||
         preference == background_key.identity() ||
         preference == chrome_key.identity();
}

}
