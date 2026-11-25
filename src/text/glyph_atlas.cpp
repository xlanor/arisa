#include <arisa/text/glyph_atlas.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <limits>
#include <utility>

namespace arisa::text {

auto GlyphAtlas::KeyHash::operator()(const Key& key) const noexcept
    -> std::size_t {
  std::size_t seed = std::hash<std::uint64_t>{}(key.face);
  for (const std::uint32_t value : {key.glyph, key.size, key.bin}) {
    seed ^= std::hash<std::uint32_t>{}(value) + 0x9E3779B97F4A7C15ULL +
            (seed << 6U) + (seed >> 2U);
  }
  return seed;
}

GlyphAtlas::GlyphAtlas(provider::GpuProvider& gpu,
                       AtlasOptions options) noexcept
    : gpu_(gpu), options_(options) {}

auto GlyphAtlas::valid(const AtlasOptions& options) noexcept -> bool {
  return options.page_size >= 64 &&
         options.page_size <= provider::Texture::maximum_extent &&
         options.maximum_pages >= 1 && options.maximum_pages <= 64 &&
         options.subpixel_steps >= 1 && options.subpixel_steps <= 16 &&
         options.padding <= 4;
}

auto GlyphAtlas::create(provider::GpuProvider& gpu, AtlasOptions options)
    -> std::expected<GlyphAtlas, TextError> {
  if (!valid(options)) {
    return std::unexpected(TextError{TextErrorCode::invalid_argument});
  }
  GlyphAtlas atlas{gpu, options};
  if (auto added = atlas.add_page(); !added.has_value()) {
    return std::unexpected(added.error());
  }
  return atlas;
}

auto GlyphAtlas::add_page() -> std::expected<void, TextError> {
  auto texture = gpu_.get().create_texture(
      provider::TextureDescription{.width = options_.page_size,
                                   .height = options_.page_size,
                                   .format = provider::TextureFormat::alpha8});
  if (!texture.has_value()) {
    return std::unexpected(TextError::from(texture.error()));
  }
  pages_.emplace_back(std::move(*texture), options_.page_size);
  return {};
}

void GlyphAtlas::retain(std::uint64_t pages) noexcept {
  for (std::uint32_t index = 0;
       index < pages_.size() &&
       index < std::numeric_limits<std::uint64_t>::digits;
       ++index) {
    if (((pages >> index) & 1U) != 0) {
      pages_[index].touch(frame_);
    }
  }
}

auto GlyphAtlas::evict() -> std::optional<std::uint32_t> {
  std::optional<std::uint32_t> oldest;
  for (std::uint32_t index = 0; index < pages_.size(); ++index) {
    if (pages_[index].last_used() >= frame_) {
      continue;
    }
    if (!oldest.has_value() ||
        pages_[index].last_used() < pages_[*oldest].last_used()) {
      oldest = index;
    }
  }
  if (!oldest.has_value()) {
    return std::nullopt;
  }
  std::erase_if(entries_, [page = *oldest](const auto& item) {
    return !item.second.blank && item.second.page == page;
  });
  pages_[*oldest].reset();
  if (*oldest < std::numeric_limits<std::uint64_t>::digits) {
    evicted_ |= std::uint64_t{1} << *oldest;
  }
  ++statistics_.evictions;
  return oldest;
}

auto GlyphAtlas::insert(const GlyphBitmap& bitmap)
    -> std::expected<Entry, TextError> {
  if (bitmap.empty()) {
    return Entry{};
  }
  const std::uint32_t width = bitmap.width() + (2 * options_.padding);
  const std::uint32_t height = bitmap.height() + (2 * options_.padding);
  if (width > options_.page_size || height > options_.page_size) {
    return std::unexpected(TextError{TextErrorCode::atlas_full});
  }
  std::optional<AtlasPage::Slot> slot;
  std::uint32_t page = 0;
  for (; page < pages_.size(); ++page) {
    slot = pages_[page].allocate(width, height);
    if (slot.has_value()) {
      break;
    }
  }
  if (!slot.has_value() && pages_.size() < options_.maximum_pages) {
    if (auto added = add_page(); !added.has_value()) {
      return std::unexpected(added.error());
    }
    page = static_cast<std::uint32_t>(pages_.size() - 1);
    slot = pages_[page].allocate(width, height);
  }
  if (!slot.has_value()) {
    if (const auto evicted = evict()) {
      page = *evicted;
      slot = pages_[page].allocate(width, height);
    }
  }
  if (!slot.has_value()) {
    return std::unexpected(TextError{TextErrorCode::atlas_full});
  }
  pages_[page].write(*slot, options_.padding, bitmap);
  return Entry{.page = page,
               .slot = *slot,
               .left = bitmap.left(),
               .top = bitmap.top(),
               .width = bitmap.width(),
               .height = bitmap.height(),
               .blank = false};
}

auto GlyphAtlas::quad(const Entry& entry, float left, float top) const noexcept
    -> GlyphQuad {
  const AtlasPage& page = pages_[entry.page];
  const auto extent = static_cast<float>(page.size());
  const auto x = static_cast<float>(entry.slot.x + options_.padding);
  const auto y = static_cast<float>(entry.slot.y + options_.padding);
  const auto width = static_cast<float>(entry.width);
  const auto height = static_cast<float>(entry.height);
  return GlyphQuad{.texture = page.texture(),
                   .page = entry.page,
                   .left = left,
                   .top = top,
                   .right = left + width,
                   .bottom = top + height,
                   .u0 = x / extent,
                   .v0 = y / extent,
                   .u1 = (x + width) / extent,
                   .v1 = (y + height) / extent};
}

auto GlyphAtlas::place(const FontFace& face, GlyphId glyph, PointSize size,
                       provider::SurfaceScale scale, float x, float y,
                       bool quantized)
    -> std::expected<std::optional<GlyphQuad>, TextError> {
  if (!std::isfinite(x) || !std::isfinite(y)) {
    return std::unexpected(TextError{TextErrorCode::invalid_argument});
  }
  const float steps = static_cast<float>(options_.subpixel_steps);
  const float device_x = x * scale.value();
  const float device_y = y * scale.value();
  float whole = quantized ? std::floor(device_x) : device_x;
  auto bin =
      static_cast<std::uint32_t>(std::lround((device_x - whole) * steps));
  if (bin >= options_.subpixel_steps) {
    whole += 1.0F;
    bin = 0;
  }
  const float pixel_size = size.value() * scale.value();
  const Key key{
      .face = face.id().value(),
      .glyph = glyph.value(),
      .size = static_cast<std::uint32_t>(std::lround(pixel_size * 64.0F)),
      .bin = bin};
  auto found = entries_.find(key);
  if (found == entries_.end()) {
    auto bitmap =
        face.rasterise(glyph, pixel_size, static_cast<float>(bin) / steps);
    if (!bitmap.has_value()) {
      return std::unexpected(bitmap.error());
    }
    auto entry = insert(*bitmap);
    if (!entry.has_value()) {
      return std::unexpected(entry.error());
    }
    ++statistics_.rasterised;
    found = entries_.emplace(key, *entry).first;
  }
  const Entry& entry = found->second;
  if (entry.blank) {
    return std::optional<GlyphQuad>{};
  }
  pages_[entry.page].touch(frame_);
  return quad(entry, whole + static_cast<float>(entry.left),
              (quantized ? std::round(device_y) : device_y) -
                  static_cast<float>(entry.top));
}

auto GlyphAtlas::mask(std::uint64_t key,
                      std::move_only_function<GlyphBitmap()> rasterise)
    -> std::expected<std::optional<GlyphQuad>, TextError> {
  const Key wanted{.face = mask_face,
                   .glyph = static_cast<std::uint32_t>(key & 0xFFFFFFFFU),
                   .size = static_cast<std::uint32_t>(key >> 32U),
                   .bin = 0};
  auto found = entries_.find(wanted);
  if (found == entries_.end()) {
    auto entry = insert(rasterise());
    if (!entry.has_value()) {
      return std::unexpected(entry.error());
    }
    ++statistics_.rasterised;
    found = entries_.emplace(wanted, *entry).first;
  }
  const Entry& entry = found->second;
  if (entry.blank) {
    return std::optional<GlyphQuad>{};
  }
  pages_[entry.page].touch(frame_);
  return quad(entry, 0.0F, 0.0F);
}

auto GlyphAtlas::flush() -> std::expected<void, TextError> {
  for (AtlasPage& page : pages_) {
    auto uploaded = page.upload(gpu_.get());
    if (!uploaded.has_value()) {
      return std::unexpected(TextError::from(uploaded.error()));
    }
    statistics_.uploads += uploaded->regions;
    statistics_.uploaded_bytes += uploaded->bytes;
  }
  return {};
}

auto GlyphAtlas::statistics() const noexcept -> AtlasStatistics {
  AtlasStatistics current = statistics_;
  current.glyphs = entries_.size();
  current.pages = pages_.size();
  return current;
}

}
