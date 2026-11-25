#include <interaction_screen.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>

#include <gallery_wallpaper.hpp>

#include <format>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

const composition::StateKey<std::string> InteractionScreen::Stage::typed{
    "bench-typed", std::string{}};

auto InteractionScreen::body(composition::Context&) const -> Element {
  Element content =
      VStack(alignment::leading, spacing(theme::space::lg),
             padding(theme::space::lg),
             frame({.max_width = infinity,
                    .max_height = infinity,
                    .alignment = composition::Alignment::leading}))(
          Element{Stage{mode_, remote_}}, Spacer(), tiles())
          .navigation_title("Interaction");
  return Scaffold(std::move(content))
      .background(Element{GalleryWallpaper{GalleryWallpaper::Look::window}});
}

auto InteractionScreen::tiles() -> Element {
  std::vector<Element> tiles;
  for (const theme::Material material : theme::Material::all()) {
    tiles.push_back(
        Text(font::callout)(std::string{material.name()})
            .padding(theme::space::md)
            .frame({.width = tile_width, .height = tile_height})
            .background(material,
                        RoundedRectangle(theme::radius::card,
                                         RoundedCornerStyle::continuous)));
  }
  return Element{HStack(alignment::center, spacing(theme::space::md),
                        wrap)(std::move(tiles))}
      .padding(theme::space::lg)
      .frame({.max_width = infinity})
      .background(Element{GalleryWallpaper{GalleryWallpaper::Look::busy}})
      .clip_shape(RoundedRectangle(theme::radius::card,
                                   RoundedCornerStyle::continuous));
}

auto InteractionScreen::Stage::body(composition::Context& context) const
    -> Element {
  switch (mode_) {
    case Mode::toggle:
      return card(Toggle("Wi-Fi", context.state(flag)).id(control));
    case Mode::slider:
      return card(
          Slider(context.state(level),
                 ClosedRange<double>{.lower_bound = 0.0, .upper_bound = 1.0},
                 slider_step)
              .id(control));
    case Mode::list:
      return rows(remote_);
    case Mode::menu:
      return card(Menu("Actions", [remote = remote_] {
                    return actions(remote);
                  }).id(control));
    case Mode::spinner:
      return card(ProgressView().id(control));
    case Mode::text: {
      const auto text = context.state(typed);
      remote_->text = text;
      return card(TextField("Name", text).id(control));
    }
    case Mode::clock:
      return card(Text("Waiting for the minute").id(control));
  }
  return card(Text("Waiting for the minute").id(control));
}

auto InteractionScreen::Stage::card(Element content) -> Element {
  return Element{
      VStack(alignment::leading, spacing(theme::space::md))(std::move(content))}
      .padding(theme::space::lg)
      .frame({.width = card_width})
      .glass_effect(
          theme::Glass::regular(),
          RoundedRectangle(theme::radius::card, RoundedCornerStyle::continuous))
      .id(stage);
}

auto InteractionScreen::Stage::rows(const std::shared_ptr<Remote>& remote)
    -> Element {
  return Element{
      VStack(
          List(ForEach(
                   std::views::iota(0, list_rows), [](int row) { return row; },
                   [remote](int row) {
                     return Button(action([remote] { ++remote->pressed; }))(
                         Label(std::format("Material {}", row),
                               Element{Image(system_name{"layers"})}));
                   }))
              .id(control))}
      .frame({.width = card_width, .height = list_height})
      .background(
          theme::Material::thin,
          RoundedRectangle(theme::radius::card, RoundedCornerStyle::continuous))
      .id(stage);
}

auto InteractionScreen::Stage::actions(const std::shared_ptr<Remote>& remote)
    -> Element {
  return Group(Button(action([remote] { ++remote->pressed; }))("Duplicate"),
               Button(action([remote] { ++remote->pressed; }))("Rename"),
               Button(role(ButtonRole::destructive),
                      action([remote] { ++remote->pressed; }))("Delete"));
}

}
