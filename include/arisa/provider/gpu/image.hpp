#pragma once

#include <arisa/provider/error.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace arisa::provider {

class Image {
 public:
  static constexpr std::size_t channels = 4;

  [[nodiscard]] static auto create(std::uint32_t width, std::uint32_t height,
                                   std::vector<std::byte> rgba)
      -> std::expected<Image, ProviderError>;

  [[nodiscard]] static auto decode(std::span<const std::byte> encoded)
      -> std::expected<Image, ProviderError>;

  auto width() const noexcept -> std::uint32_t { return width_; }

  auto height() const noexcept -> std::uint32_t { return height_; }

  auto pixels() const noexcept -> std::span<const std::byte> { return rgba_; }

  auto pixel(std::uint32_t x, std::uint32_t y) const noexcept
      -> std::optional<std::array<std::uint8_t, channels>>;

 private:
  struct DecodedTraits {
    using handle_type = unsigned char*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle != nullptr;
    }

    static void release(handle_type& handle) noexcept;
  };

  Image(std::uint32_t width, std::uint32_t height,
        std::vector<std::byte> rgba) noexcept;

  std::uint32_t width_;
  std::uint32_t height_;
  std::vector<std::byte> rgba_;
};

}
