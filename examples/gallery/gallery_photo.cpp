#include <gallery_photo.hpp>

#include <arisa/provider/assets/asset_path.hpp>

#include <utility>

namespace arisa::examples {

auto GalleryPhoto::load(const provider::AssetProvider& assets,
                        provider::SurfaceSize window)
    -> std::shared_ptr<const provider::Image> {
  const bool large = window.width.value() > small_width ||
                     window.height.value() > (small_width * 9U) / 16U;
  if (auto chosen = read(assets, large ? large_path : small_path)) {
    return chosen;
  }
  return read(assets, large ? small_path : large_path);
}

auto GalleryPhoto::read(const provider::AssetProvider& assets,
                        std::string_view name)
    -> std::shared_ptr<const provider::Image> {
  const auto path = provider::AssetPath::create(name);
  if (!path.has_value() || !assets.exists(*path)) {
    return nullptr;
  }
  const auto blob = assets.read(*path);
  if (!blob.has_value()) {
    return nullptr;
  }
  auto decoded = provider::Image::decode(blob->bytes());
  if (!decoded.has_value()) {
    return nullptr;
  }
  return std::make_shared<const provider::Image>(std::move(*decoded));
}

}
