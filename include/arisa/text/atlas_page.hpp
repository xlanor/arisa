#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/text/glyph_bitmap.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace arisa::text {

class AtlasPage {
 public:
  struct Uploaded {
    std::size_t regions{0};
    std::size_t bytes{0};
  };

  struct Slot {
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
  };

  AtlasPage(provider::Texture texture, std::uint32_t size);

  auto texture() const noexcept -> provider::TextureId { return texture_.id(); }

  auto size() const noexcept -> std::uint32_t { return size_; }

  auto pixels() const noexcept -> std::span<const std::byte> { return pixels_; }

  auto coverage(std::uint32_t x, std::uint32_t y) const noexcept
      -> std::uint8_t;

  auto last_used() const noexcept -> std::uint64_t { return last_used_; }

  auto allocate(std::uint32_t width, std::uint32_t height)
      -> std::optional<Slot>;

  void write(const Slot& slot, std::uint32_t padding,
             const GlyphBitmap& bitmap);

  void touch(std::uint64_t frame) noexcept { last_used_ = frame; }

  void reset() noexcept;

  auto dirty() const noexcept -> bool;

  [[nodiscard]] auto upload(provider::GpuProvider& gpu)
      -> std::expected<Uploaded, provider::ProviderError>;

 private:
  struct Shelf {
    std::uint32_t y{0};
    std::uint32_t height{0};
    std::uint32_t used{0};
    std::uint32_t dirty_begin{0};
    std::uint32_t dirty_end{0};
  };

  provider::Texture texture_;
  std::uint32_t size_;
  std::vector<std::byte> pixels_;
  std::vector<Shelf> shelves_;
  std::uint32_t next_y_{0};
  std::uint64_t last_used_{0};
};

}
