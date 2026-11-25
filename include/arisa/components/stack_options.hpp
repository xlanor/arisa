#pragma once

#include <arisa/components/alignment.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/pinned_scrollable_views.hpp>
#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/theme/space.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace arisa::components {

struct VStackOptions {
  HorizontalAlignment alignment{HorizontalAlignment::center};
  std::optional<theme::Space> spacing{};
  Distribution distribution{Distribution::start};
  bool wrap{false};
  std::optional<composition::AlignmentKey> guide{};

  friend constexpr auto operator==(const VStackOptions&, const VStackOptions&)
      -> bool = default;
};

struct HStackOptions {
  VerticalAlignment alignment{VerticalAlignment::center};
  std::optional<theme::Space> spacing{};
  Distribution distribution{Distribution::start};
  bool wrap{false};
  std::optional<composition::AlignmentKey> guide{};

  friend constexpr auto operator==(const HStackOptions&, const HStackOptions&)
      -> bool = default;
};

struct ZStackOptions {
  Alignment alignment{};
  std::optional<composition::AlignmentKey> horizontal_guide{};
  std::optional<composition::AlignmentKey> vertical_guide{};

  friend constexpr auto operator==(const ZStackOptions&, const ZStackOptions&)
      -> bool = default;
};

struct LazyVStackOptions {
  HorizontalAlignment alignment{HorizontalAlignment::center};
  theme::Space spacing{theme::space::sm};
  float overscan{240.0F};
  std::size_t retention{4};
  float estimated_extent{44.0F};
  PinnedScrollableViews pinned_views{};

  friend constexpr auto operator==(const LazyVStackOptions&,
                                   const LazyVStackOptions&) -> bool = default;
};

struct LazyHStackOptions {
  VerticalAlignment alignment{VerticalAlignment::center};
  theme::Space spacing{theme::space::sm};
  float overscan{240.0F};
  std::size_t retention{4};
  float estimated_extent{120.0F};
  PinnedScrollableViews pinned_views{};

  friend constexpr auto operator==(const LazyHStackOptions&,
                                   const LazyHStackOptions&) -> bool = default;
};

struct LazyVGridOptions {
  std::vector<GridItem> columns{GridItem{}};
  HorizontalAlignment alignment{HorizontalAlignment::center};
  std::optional<theme::Space> spacing{};
  float overscan{240.0F};
  std::size_t retention{2};
  float estimated_extent{120.0F};
  PinnedScrollableViews pinned_views{};

  friend auto operator==(const LazyVGridOptions&, const LazyVGridOptions&)
      -> bool = default;
};

struct LazyHGridOptions {
  std::vector<GridItem> rows{GridItem{}};
  VerticalAlignment alignment{VerticalAlignment::center};
  std::optional<theme::Space> spacing{};
  float overscan{240.0F};
  std::size_t retention{2};
  float estimated_extent{120.0F};
  PinnedScrollableViews pinned_views{};

  friend auto operator==(const LazyHGridOptions&, const LazyHGridOptions&)
      -> bool = default;
};

struct ScrollViewOptions {
  composition::Axes axes{composition::Axes::vertical};
  bool shows_indicators{true};

  friend constexpr auto operator==(const ScrollViewOptions&,
                                   const ScrollViewOptions&) -> bool = default;
};

}
