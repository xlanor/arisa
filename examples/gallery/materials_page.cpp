#include <materials_page.hpp>

#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <format>
#include <utility>
#include <vector>

namespace arisa::examples {

auto MaterialsPage::tile_id(theme::Material material,
                            GalleryWallpaper::Look look) -> std::string {
  return std::format(
      "material-{}-{}", material.name(),
      look == GalleryWallpaper::Look::launch ? "wallpaper" : "busy");
}

auto MaterialsPage::tile(theme::Material material, GalleryWallpaper::Look look)
    -> composition::Element {
  composition::Element label =
      components::Text(std::string{material.name()}).font(theme::Font::callout);
  if (material == theme::Material::hud) {
    label = std::move(label).foreground_style(
        theme::ColourValue{theme::Colour::hex(0xFFFFFF)});
  }
  return std::move(label)
      .padding(theme::space::md)
      .frame({.width = 118.0F, .height = 64.0F})
      .background(material, theme::RoundedRectangle(
                                theme::radius::card,
                                theme::RoundedCornerStyle::continuous))
      .id(tile_id(material, look));
}

auto MaterialsPage::row(GalleryWallpaper::Look look) -> composition::Element {
  std::vector<composition::Element> tiles;
  for (const theme::Material material : theme::Material::all()) {
    tiles.push_back(tile(material, look));
  }
  return composition::Element{
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::md,
              .distribution = components::Distribution::start,
              .wrap = true},
          std::move(tiles))}
      .padding(theme::space::lg)
      .frame({.max_width = composition::infinity})
      .background(composition::Element{GalleryWallpaper{look}})
      .clip_shape(theme::RoundedRectangle(
          theme::radius::card, theme::RoundedCornerStyle::continuous))
      .id(look == GalleryWallpaper::Look::launch ? wallpaper_row : busy_row);
}

auto MaterialsPage::body(composition::Context&) const -> composition::Element {
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::md},
                 components::Text("Over the wallpaper")
                     .font(theme::Font::headline),
                 row(GalleryWallpaper::Look::launch),
                 components::Text("Over busy content")
                     .font(theme::Font::headline),
                 row(GalleryWallpaper::Look::busy))
                 .frame({.max_width = composition::infinity,
                         .alignment = composition::Alignment::leading}))
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity});
}

}
