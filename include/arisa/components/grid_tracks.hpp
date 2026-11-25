#pragma once

#include <arisa/components/alignment.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/theme/theme.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace arisa::components {

class GridTracks {
 public:
  struct Track {
    float offset{0.0F};
    float length{0.0F};
    std::optional<Alignment> alignment{};

    friend auto operator==(const Track&, const Track&) -> bool = default;
  };

  GridTracks(std::span<const GridItem> items, std::span<const float> spacings,
             float available);

  static auto spacing(const GridItem& item, const theme::Theme& theme) noexcept
      -> float;

  static auto spacings(std::span<const GridItem> items,
                       const theme::Theme& theme) -> std::vector<float>;

  static auto count(std::span<const GridItem> items, float available,
                    const theme::Theme& theme) noexcept -> std::size_t;

  static auto used(std::span<const GridItem> items,
                   std::span<const float> spacings, float available) noexcept
      -> float;

  auto count() const noexcept -> std::size_t { return tracks_.size(); }

  auto tracks() const noexcept -> std::span<const Track> { return tracks_; }

  auto used() const noexcept -> float { return used_; }

  auto extent() const noexcept -> float { return extent_; }

 private:
  struct Share {
    std::size_t columns{1};
    float length{0.0F};
  };

  static auto definite(float available) noexcept -> bool;

  static auto divide(const GridItem::Size& size, float share, float spacing,
                     bool known) noexcept -> Share;

  template <class SpacingOf, class Visit>
  static auto resolve(std::span<const GridItem> items, SpacingOf spacing_of,
                      float available, Visit visit) -> float;

  std::vector<Track> tracks_;
  float used_{0.0F};
  float extent_{0.0F};
};

}
