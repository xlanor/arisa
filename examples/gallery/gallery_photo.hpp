#pragma once

#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <cstdint>
#include <memory>
#include <string_view>

namespace arisa::examples {

class GalleryPhoto {
 public:
  static constexpr std::string_view large_path = "arisa/images/london_1920.jpg";
  static constexpr std::string_view small_path = "arisa/images/london_1280.jpg";
  static constexpr std::uint32_t small_width = 1280;
  static constexpr std::uint32_t average = 0x8DA6A6;

  static auto load(const provider::AssetProvider& assets,
                   provider::SurfaceSize window)
      -> std::shared_ptr<const provider::Image>;

 private:
  static auto read(const provider::AssetProvider& assets, std::string_view name)
      -> std::shared_ptr<const provider::Image>;
};

}
