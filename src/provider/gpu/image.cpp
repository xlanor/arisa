#include <arisa/provider/gpu/image.hpp>
#include <arisa/support/unique_handle.hpp>

#include <stb_image.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace arisa::provider {

Image::Image(std::uint32_t width, std::uint32_t height,
             std::vector<std::byte> rgba) noexcept
    : width_(width), height_(height), rgba_(std::move(rgba)) {}

auto Image::create(std::uint32_t width, std::uint32_t height,
                   std::vector<std::byte> rgba)
    -> std::expected<Image, ProviderError> {
  if (width == 0 || height == 0 ||
      rgba.size() != static_cast<std::size_t>(width) * height * channels) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return Image{width, height, std::move(rgba)};
}

void Image::DecodedTraits::release(handle_type& handle) noexcept {
  stbi_image_free(handle);
}

auto Image::decode(std::span<const std::byte> encoded)
    -> std::expected<Image, ProviderError> {
  if (encoded.empty() ||
      encoded.size() >
          static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  int width = 0;
  int height = 0;
  int components = 0;
  const support::UniqueHandle<DecodedTraits> decoded{stbi_load_from_memory(
      reinterpret_cast<const unsigned char*>(encoded.data()),
      static_cast<int>(encoded.size()), &width, &height, &components,
      static_cast<int>(channels))};
  if (!decoded.valid() || width <= 0 || height <= 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  const std::size_t size = static_cast<std::size_t>(width) *
                           static_cast<std::size_t>(height) * channels;
  std::vector<std::byte> rgba(size);
  std::ranges::transform(
      std::span<const unsigned char>{decoded.get(), size}, rgba.begin(),
      [](unsigned char value) { return static_cast<std::byte>(value); });
  return create(static_cast<std::uint32_t>(width),
                static_cast<std::uint32_t>(height), std::move(rgba));
}

auto Image::pixel(std::uint32_t x, std::uint32_t y) const noexcept
    -> std::optional<std::array<std::uint8_t, channels>> {
  if (x >= width_ || y >= height_) {
    return std::nullopt;
  }
  const std::size_t offset =
      (static_cast<std::size_t>(y) * width_ + x) * channels;
  return std::array{std::to_integer<std::uint8_t>(rgba_[offset]),
                    std::to_integer<std::uint8_t>(rgba_[offset + 1]),
                    std::to_integer<std::uint8_t>(rgba_[offset + 2]),
                    std::to_integer<std::uint8_t>(rgba_[offset + 3])};
}

}
