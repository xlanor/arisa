#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/theme/material.hpp>

#include <gallery_wallpaper.hpp>

#include <string>
#include <string_view>

namespace arisa::examples {

class MaterialsPage final : public composition::Component {
 public:
  static constexpr std::string_view wallpaper_row = "materials-wallpaper";
  static constexpr std::string_view busy_row = "materials-busy";

  auto operator==(const MaterialsPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto tile_id(theme::Material material, GalleryWallpaper::Look look)
      -> std::string;

 private:
  static auto row(GalleryWallpaper::Look look) -> composition::Element;

  static auto tile(theme::Material material, GalleryWallpaper::Look look)
      -> composition::Element;
};

}
