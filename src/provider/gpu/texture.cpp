#include <arisa/provider/gpu/texture.hpp>

#include <utility>

namespace arisa::provider {

Texture::Texture(TextureId id, TextureDescription description,
                 support::ScopedCallback release) noexcept
    : id_(id), description_(description), release_(std::move(release)) {}

auto Texture::describable(const TextureDescription& description) noexcept
    -> bool {
  return description.width > 0 && description.height > 0 &&
         description.width <= maximum_extent &&
         description.height <= maximum_extent &&
         description.format != TextureFormat::nv12 &&
         (description.usage == TextureUsage::sampled ||
          description.format == TextureFormat::rgba8);
}

auto Texture::upload_size(const TextureDescription& description,
                          const TextureRegion& region) noexcept
    -> std::optional<std::size_t> {
  if (bytes_per_pixel(description.format) == 0 || region.width == 0 ||
      region.height == 0 || region.x >= description.width ||
      region.y >= description.height ||
      region.width > description.width - region.x ||
      region.height > description.height - region.y) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(region.width) * region.height *
         bytes_per_pixel(description.format);
}

auto Texture::accepts(const TextureDescription& description,
                      const TextureRegion& region,
                      const PixelRows& pixels) noexcept -> bool {
  return upload_size(description, region).has_value() &&
         pixels.covers(static_cast<std::size_t>(region.width) *
                           bytes_per_pixel(description.format),
                       region.height);
}

}
