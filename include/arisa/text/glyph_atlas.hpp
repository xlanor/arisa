#pragma once

#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/text/atlas_options.hpp>
#include <arisa/text/atlas_page.hpp>
#include <arisa/text/atlas_statistics.hpp>
#include <arisa/text/font_face.hpp>
#include <arisa/text/glyph_bitmap.hpp>
#include <arisa/text/glyph_id.hpp>
#include <arisa/text/glyph_quad.hpp>
#include <arisa/text/point_size.hpp>
#include <arisa/text/text_error.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace arisa::text {

class GlyphAtlas {
 public:
  [[nodiscard]] static auto create(provider::GpuProvider& gpu,
                                   AtlasOptions options = {})
      -> std::expected<GlyphAtlas, TextError>;

  [[nodiscard]] auto place(const FontFace& face, GlyphId glyph, PointSize size,
                           provider::SurfaceScale scale, float x, float y,
                           bool quantized = true)
      -> std::expected<std::optional<GlyphQuad>, TextError>;

  [[nodiscard]] auto mask(std::uint64_t key,
                          std::move_only_function<GlyphBitmap()> rasterise)
      -> std::expected<std::optional<GlyphQuad>, TextError>;

  [[nodiscard]] auto flush() -> std::expected<void, TextError>;

  void advance_frame() noexcept {
    ++frame_;
    evicted_ = 0;
  }

  auto evicted_pages() const noexcept -> std::uint64_t { return evicted_; }

  void retain(std::uint64_t pages) noexcept;

  auto pages() const noexcept -> std::span<const AtlasPage> { return pages_; }

  auto statistics() const noexcept -> AtlasStatistics;

  auto options() const noexcept -> const AtlasOptions& { return options_; }

 private:
  static constexpr std::uint64_t mask_face = ~std::uint64_t{0};

  struct Key {
    std::uint64_t face{0};
    std::uint32_t glyph{0};
    std::uint32_t size{0};
    std::uint32_t bin{0};

    friend auto operator==(const Key&, const Key&) -> bool = default;
  };

  struct KeyHash {
    auto operator()(const Key& key) const noexcept -> std::size_t;
  };

  struct Entry {
    std::uint32_t page{0};
    AtlasPage::Slot slot;
    std::int32_t left{0};
    std::int32_t top{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    bool blank{true};
  };

  GlyphAtlas(provider::GpuProvider& gpu, AtlasOptions options) noexcept;

  static auto valid(const AtlasOptions& options) noexcept -> bool;

  [[nodiscard]] auto insert(const GlyphBitmap& bitmap)
      -> std::expected<Entry, TextError>;

  [[nodiscard]] auto add_page() -> std::expected<void, TextError>;

  auto evict() -> std::optional<std::uint32_t>;

  auto quad(const Entry& entry, float left, float top) const noexcept
      -> GlyphQuad;

  std::reference_wrapper<provider::GpuProvider> gpu_;
  AtlasOptions options_;
  std::vector<AtlasPage> pages_;
  std::unordered_map<Key, Entry, KeyHash> entries_;
  std::uint64_t frame_{1};
  std::uint64_t evicted_{0};
  AtlasStatistics statistics_;
};

}
