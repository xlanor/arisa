#include <arisa/text/atlas_page.hpp>

#include <algorithm>
#include <utility>

namespace arisa::text {

AtlasPage::AtlasPage(provider::Texture texture, std::uint32_t size)
    : texture_(std::move(texture)),
      size_(size),
      pixels_(static_cast<std::size_t>(size) * size, std::byte{0}) {}

auto AtlasPage::coverage(std::uint32_t x, std::uint32_t y) const noexcept
    -> std::uint8_t {
  if (x >= size_ || y >= size_) {
    return 0;
  }
  return std::to_integer<std::uint8_t>(
      pixels_[(static_cast<std::size_t>(y) * size_) + x]);
}

auto AtlasPage::allocate(std::uint32_t width, std::uint32_t height)
    -> std::optional<Slot> {
  if (width == 0 || height == 0 || width > size_ || height > size_) {
    return std::nullopt;
  }
  Shelf* best = nullptr;
  for (Shelf& shelf : shelves_) {
    const bool fits = shelf.height >= height &&
                      shelf.height <= height + (height / 2) + 2 &&
                      size_ - shelf.used >= width;
    if (fits && (best == nullptr || shelf.height < best->height)) {
      best = &shelf;
    }
  }
  if (best == nullptr) {
    if (size_ - next_y_ < height) {
      return std::nullopt;
    }
    best = &shelves_.emplace_back(Shelf{.y = next_y_, .height = height});
    next_y_ += height;
  }
  const Slot slot{
      .x = best->used, .y = best->y, .width = width, .height = height};
  best->used += width;
  return slot;
}

void AtlasPage::write(const Slot& slot, std::uint32_t padding,
                      const GlyphBitmap& bitmap) {
  const std::span<const std::byte> source = bitmap.coverage();
  for (std::uint32_t row = 0; row < bitmap.height(); ++row) {
    const std::size_t target =
        (static_cast<std::size_t>(slot.y + padding + row) * size_) + slot.x +
        padding;
    std::ranges::copy(
        source.subspan(static_cast<std::size_t>(row) * bitmap.width(),
                       bitmap.width()),
        pixels_.begin() + static_cast<std::ptrdiff_t>(target));
  }
  const auto shelf = std::ranges::find_if(
      shelves_, [&slot](const Shelf& entry) { return entry.y == slot.y; });
  if (shelf == shelves_.end()) {
    return;
  }
  if (shelf->dirty_end <= shelf->dirty_begin) {
    shelf->dirty_begin = slot.x;
    shelf->dirty_end = slot.x + slot.width;
  } else {
    shelf->dirty_begin = std::min(shelf->dirty_begin, slot.x);
    shelf->dirty_end = std::max(shelf->dirty_end, slot.x + slot.width);
  }
}

void AtlasPage::reset() noexcept {
  std::ranges::fill(pixels_, std::byte{0});
  shelves_.clear();
  next_y_ = 0;
}

auto AtlasPage::dirty() const noexcept -> bool {
  return std::ranges::any_of(shelves_, [](const Shelf& shelf) {
    return shelf.dirty_end > shelf.dirty_begin;
  });
}

auto AtlasPage::upload(provider::GpuProvider& gpu)
    -> std::expected<Uploaded, provider::ProviderError> {
  Uploaded uploaded;
  for (Shelf& shelf : shelves_) {
    if (shelf.dirty_end <= shelf.dirty_begin) {
      continue;
    }
    const provider::TextureRegion region{
        .x = shelf.dirty_begin,
        .y = shelf.y,
        .width = shelf.dirty_end - shelf.dirty_begin,
        .height = shelf.height};
    const std::size_t first =
        (static_cast<std::size_t>(shelf.y) * size_) + shelf.dirty_begin;
    if (auto updated =
            gpu.update_texture(texture_, region,
                               provider::PixelRows::strided(
                                   std::span{pixels_}.subspan(first), size_));
        !updated.has_value()) {
      return std::unexpected(updated.error());
    }
    shelf.dirty_begin = 0;
    shelf.dirty_end = 0;
    ++uploaded.regions;
    uploaded.bytes += static_cast<std::size_t>(region.width) * region.height;
  }
  return uploaded;
}

}
