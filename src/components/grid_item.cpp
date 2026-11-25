#include <arisa/components/grid_item.hpp>
#include <arisa/components/grid_tracks.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::components {

auto GridItem::repeated(std::size_t count, GridItem item)
    -> std::vector<GridItem> {
  return std::vector<GridItem>(std::max<std::size_t>(count, 1), item);
}

auto GridTracks::definite(float available) noexcept -> bool {
  return std::isfinite(available) && available >= 0.0F;
}

auto GridTracks::divide(const GridItem::Size& size, float share, float spacing,
                        bool known) noexcept -> Share {
  switch (size.kind()) {
    case GridItem::Size::Kind::fixed:
      return Share{.columns = 1, .length = size.minimum()};
    case GridItem::Size::Kind::flexible:
      if (!known) {
        return Share{.columns = 1, .length = size.minimum()};
      }
      return Share{.columns = 1,
                   .length = std::clamp(share, size.minimum(), size.maximum())};
    case GridItem::Size::Kind::adaptive: {
      if (!known) {
        return Share{.columns = 1, .length = size.minimum()};
      }
      const float room = std::max(share, 0.0F);
      const float pitch = std::max(size.minimum() + spacing, 1.0e-3F);
      const auto fitted = static_cast<std::size_t>(
          std::max(std::floor((room + spacing) / pitch), 1.0F));
      const float gaps = spacing * static_cast<float>(fitted - 1);
      const float each =
          std::max((room - gaps) / static_cast<float>(fitted), 0.0F);
      return Share{.columns = fitted, .length = std::min(each, size.maximum())};
    }
  }
  return Share{};
}

template <class SpacingOf, class Visit>
auto GridTracks::resolve(std::span<const GridItem> items, SpacingOf spacing_of,
                         float available, Visit visit) -> float {
  const bool known = definite(available);
  float remaining = known ? available : 0.0F;
  std::size_t open = 0;
  for (std::size_t index = 0; index < items.size(); ++index) {
    const GridItem::Size& size = items[index].size();
    if (size.kind() == GridItem::Size::Kind::fixed) {
      remaining -= size.minimum();
    } else {
      ++open;
    }
    if (index + 1 < items.size()) {
      remaining -= spacing_of(index);
    }
  }
  float used = 0.0F;
  for (std::size_t index = 0; index < items.size(); ++index) {
    const GridItem& item = items[index];
    const float spacing = spacing_of(index);
    const bool fixed = item.size().kind() == GridItem::Size::Kind::fixed;
    const float share =
        fixed || open == 0 ? 0.0F : remaining / static_cast<float>(open);
    const Share divided = divide(item.size(), share, spacing, known);
    for (std::size_t column = 0; column < divided.columns; ++column) {
      visit(index, used, divided.length);
      used += divided.length;
      const bool last =
          index + 1 == items.size() && column + 1 == divided.columns;
      if (!last) {
        used += spacing;
      }
    }
    if (!fixed) {
      const float taken =
          (divided.length * static_cast<float>(divided.columns)) +
          (spacing * static_cast<float>(divided.columns - 1));
      remaining -= taken;
      --open;
    }
  }
  return used;
}

GridTracks::GridTracks(std::span<const GridItem> items,
                       std::span<const float> spacings, float available) {
  const auto spacing_of = [spacings](std::size_t index) {
    return index < spacings.size() ? spacings[index] : 0.0F;
  };
  used_ =
      resolve(items, spacing_of, available,
              [this, items](std::size_t index, float offset, float length) {
                tracks_.push_back(Track{.offset = offset,
                                        .length = length,
                                        .alignment = items[index].alignment()});
              });
  extent_ = definite(available) ? std::max(available, used_) : used_;
  const float start = std::max((extent_ - used_) * 0.5F, 0.0F);
  for (Track& track : tracks_) {
    track.offset += start;
  }
}

auto GridTracks::spacing(const GridItem& item,
                         const theme::Theme& theme) noexcept -> float {
  if (item.spacing().has_value()) {
    return std::max(theme.space(*item.spacing()), 0.0F);
  }
  return theme.scaled(theme::metric::stack_spacing);
}

auto GridTracks::spacings(std::span<const GridItem> items,
                          const theme::Theme& theme) -> std::vector<float> {
  std::vector<float> resolved;
  resolved.reserve(items.size());
  for (const GridItem& item : items) {
    resolved.push_back(spacing(item, theme));
  }
  return resolved;
}

auto GridTracks::used(std::span<const GridItem> items,
                      std::span<const float> spacings, float available) noexcept
    -> float {
  return resolve(
      items,
      [spacings](std::size_t index) {
        return index < spacings.size() ? spacings[index] : 0.0F;
      },
      available, [](std::size_t, float, float) {});
}

auto GridTracks::count(std::span<const GridItem> items, float available,
                       const theme::Theme& theme) noexcept -> std::size_t {
  std::size_t counted = 0;
  static_cast<void>(resolve(
      items,
      [items, &theme](std::size_t index) {
        return spacing(items[index], theme);
      },
      available, [&counted](std::size_t, float, float) { ++counted; }));
  return counted;
}

}
