#pragma once

#include <arisa/provider/gpu/pixel_rows.hpp>
#include <arisa/support/scoped_callback.hpp>
#include <arisa/support/strong_value.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace arisa::provider {

enum class TextureFormat : std::uint8_t {
  rgba8,
  alpha8,
  bgra8,
  rgb565,
  r8,
  rg8,
  nv12,
};

enum class ColourMatrix : std::uint8_t {
  bt601,
  bt709,
  bt2020,
};

enum class ColourRange : std::uint8_t {
  limited,
  full,
};

enum class SamplerFilter : std::uint8_t {
  linear,
  nearest,
};

enum class TextureUsage : std::uint8_t {
  sampled,
  render_target,
};

struct TextureDescription {
  std::uint32_t width{0};
  std::uint32_t height{0};
  TextureFormat format{TextureFormat::rgba8};
  TextureUsage usage{TextureUsage::sampled};
  SamplerFilter filter{SamplerFilter::linear};

  friend constexpr auto operator==(const TextureDescription&,
                                   const TextureDescription&) -> bool = default;
};

struct ImportDescription {
  std::uint32_t width{0};
  std::uint32_t height{0};
  TextureFormat format{TextureFormat::rgba8};
  SamplerFilter filter{SamplerFilter::linear};
  bool premultiplied{false};
  ColourMatrix colour_matrix{ColourMatrix::bt709};
  ColourRange range{ColourRange::limited};

  constexpr auto planes() const noexcept -> std::size_t {
    return format == TextureFormat::nv12 ? 2 : 1;
  }

  constexpr auto plane(std::size_t index) const noexcept -> TextureDescription {
    if (format != TextureFormat::nv12) {
      return TextureDescription{.width = width,
                                .height = height,
                                .format = format,
                                .usage = TextureUsage::sampled,
                                .filter = filter};
    }
    if (index == 0) {
      return TextureDescription{.width = width,
                                .height = height,
                                .format = TextureFormat::r8,
                                .usage = TextureUsage::sampled,
                                .filter = filter};
    }
    return TextureDescription{.width = (width + 1) / 2,
                              .height = (height + 1) / 2,
                              .format = TextureFormat::rg8,
                              .usage = TextureUsage::sampled,
                              .filter = filter};
  }

  constexpr auto texture() const noexcept -> TextureDescription {
    return plane(0);
  }

  friend constexpr auto operator==(const ImportDescription&,
                                   const ImportDescription&) -> bool = default;
};

struct TextureRegion {
  std::uint32_t x{0};
  std::uint32_t y{0};
  std::uint32_t width{0};
  std::uint32_t height{0};

  friend constexpr auto operator==(const TextureRegion&, const TextureRegion&)
      -> bool = default;
};

struct TextureIdTag;

using TextureId = support::StrongValue<std::uint64_t, TextureIdTag>;

class Texture {
 public:
  static constexpr std::uint32_t maximum_extent = 16384;

  Texture(TextureId id, TextureDescription description,
          support::ScopedCallback release) noexcept;

  auto id() const noexcept -> TextureId { return id_; }

  auto description() const noexcept -> const TextureDescription& {
    return description_;
  }

  auto valid() const noexcept -> bool { return release_.valid(); }

  static constexpr auto bytes_per_pixel(TextureFormat format) noexcept
      -> std::size_t {
    switch (format) {
      case TextureFormat::alpha8:
      case TextureFormat::r8:
        return 1;
      case TextureFormat::rgb565:
      case TextureFormat::rg8:
        return 2;
      case TextureFormat::nv12:
        return 0;
      case TextureFormat::rgba8:
      case TextureFormat::bgra8:
        return 4;
    }
    return 4;
  }

  static auto describable(const TextureDescription& description) noexcept
      -> bool;

  static auto upload_size(const TextureDescription& description,
                          const TextureRegion& region) noexcept
      -> std::optional<std::size_t>;

  static auto accepts(const TextureDescription& description,
                      const TextureRegion& region,
                      const PixelRows& pixels) noexcept -> bool;

 private:
  TextureId id_;
  TextureDescription description_;
  support::ScopedCallback release_;
};

}
