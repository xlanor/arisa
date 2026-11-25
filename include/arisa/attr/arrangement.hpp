#pragma once

#include <arisa/components/alignment.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/theme/space.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace arisa::attr {

class Spacing {
 public:
  constexpr explicit Spacing(theme::Space length) noexcept : length_(length) {}

  constexpr void configure(components::VStackOptions& options) const noexcept {
    options.spacing = length_;
  }

  constexpr void configure(components::HStackOptions& options) const noexcept {
    options.spacing = length_;
  }

  constexpr void configure(components::GridOptions& options) const noexcept {
    options.horizontal_spacing = length_;
    options.vertical_spacing = length_;
  }

  void configure(components::LazyVGridOptions& options) const noexcept {
    options.spacing = length_;
  }

  void configure(components::LazyHGridOptions& options) const noexcept {
    options.spacing = length_;
  }

  constexpr void configure(
      components::LazyVStackOptions& options) const noexcept {
    options.spacing = length_;
  }

  constexpr void configure(
      components::LazyHStackOptions& options) const noexcept {
    options.spacing = length_;
  }

 private:
  theme::Space length_;
};

class HorizontalSpacing {
 public:
  constexpr explicit HorizontalSpacing(theme::Space length) noexcept
      : length_(length) {}

  constexpr void configure(components::GridOptions& options) const noexcept {
    options.horizontal_spacing = length_;
  }

 private:
  theme::Space length_;
};

class VerticalSpacing {
 public:
  constexpr explicit VerticalSpacing(theme::Space length) noexcept
      : length_(length) {}

  constexpr void configure(components::GridOptions& options) const noexcept {
    options.vertical_spacing = length_;
  }

 private:
  theme::Space length_;
};

class HorizontalAlign {
 public:
  constexpr explicit HorizontalAlign(
      components::HorizontalAlignment value) noexcept
      : value_(value) {}

  constexpr void configure(components::VStackOptions& options) const noexcept {
    options.alignment = value_;
  }

  constexpr void configure(
      components::LazyVStackOptions& options) const noexcept {
    options.alignment = value_;
  }

  constexpr void configure(components::ZStackOptions& options) const noexcept {
    options.alignment.horizontal = value_;
  }

  constexpr void configure(components::GridOptions& options) const noexcept {
    options.alignment.horizontal = value_;
  }

  void configure(components::LazyVGridOptions& options) const noexcept {
    options.alignment = value_;
  }

 private:
  components::HorizontalAlignment value_;
};

class VerticalAlign {
 public:
  constexpr explicit VerticalAlign(components::VerticalAlignment value) noexcept
      : value_(value) {}

  constexpr void configure(components::HStackOptions& options) const noexcept {
    options.alignment = value_;
  }

  constexpr void configure(
      components::LazyHStackOptions& options) const noexcept {
    options.alignment = value_;
  }

  constexpr void configure(components::ZStackOptions& options) const noexcept {
    options.alignment.vertical = value_;
  }

  constexpr void configure(components::GridOptions& options) const noexcept {
    options.alignment.vertical = value_;
  }

  constexpr void configure(components::GridRowOptions& options) const noexcept {
    options.alignment = value_;
  }

  void configure(components::LazyHGridOptions& options) const noexcept {
    options.alignment = value_;
  }

 private:
  components::VerticalAlignment value_;
};

class GuideAlign {
 public:
  constexpr explicit GuideAlign(composition::AlignmentKey key) noexcept
      : key_(key) {}

  constexpr void configure(components::VStackOptions& options) const noexcept {
    options.guide = key_;
  }

  constexpr void configure(components::HStackOptions& options) const noexcept {
    options.guide = key_;
  }

 private:
  composition::AlignmentKey key_;
};

class CenterAlign {
 public:
  constexpr void configure(components::VStackOptions& options) const noexcept {
    options.alignment = components::HorizontalAlignment::center;
  }

  constexpr void configure(components::HStackOptions& options) const noexcept {
    options.alignment = components::VerticalAlignment::center;
  }

  constexpr void configure(components::ZStackOptions& options) const noexcept {
    options.alignment = components::Alignment::center;
  }

  constexpr void configure(components::GridOptions& options) const noexcept {
    options.alignment = components::Alignment::center;
  }
};

class CornerAlign {
 public:
  constexpr explicit CornerAlign(components::Alignment value) noexcept
      : value_(value) {}

  constexpr void configure(components::ZStackOptions& options) const noexcept {
    options.alignment = value_;
  }

  constexpr void configure(components::GridOptions& options) const noexcept {
    options.alignment = value_;
  }

 private:
  components::Alignment value_;
};

class Justify {
 public:
  constexpr explicit Justify(components::Distribution value) noexcept
      : value_(value) {}

  constexpr void configure(components::VStackOptions& options) const noexcept {
    options.distribution = value_;
  }

  constexpr void configure(components::HStackOptions& options) const noexcept {
    options.distribution = value_;
  }

 private:
  components::Distribution value_;
};

class Wrap {
 public:
  constexpr void configure(components::VStackOptions& options) const noexcept {
    options.wrap = true;
  }

  constexpr void configure(components::HStackOptions& options) const noexcept {
    options.wrap = true;
  }
};

class Columns {
 public:
  explicit Columns(std::size_t count)
      : items_(components::GridItem::repeated(count)) {}

  Columns(std::size_t count, theme::Space spacing)
      : items_(components::GridItem::repeated(
            count, components::GridItem{components::GridItem::Size::flexible(),
                                        spacing})) {}

  explicit Columns(std::vector<components::GridItem> items)
      : items_(std::move(items)) {}

  void configure(components::LazyVGridOptions& options) && {
    options.columns = std::move(items_);
  }

 private:
  std::vector<components::GridItem> items_;
};

class Rows {
 public:
  explicit Rows(std::size_t count)
      : items_(components::GridItem::repeated(count)) {}

  Rows(std::size_t count, theme::Space spacing)
      : items_(components::GridItem::repeated(
            count, components::GridItem{components::GridItem::Size::flexible(),
                                        spacing})) {}

  explicit Rows(std::vector<components::GridItem> items)
      : items_(std::move(items)) {}

  void configure(components::LazyHGridOptions& options) && {
    options.rows = std::move(items_);
  }

 private:
  std::vector<components::GridItem> items_;
};

using spacing = Spacing;
using horizontal_spacing = HorizontalSpacing;
using vertical_spacing = VerticalSpacing;
using columns = Columns;
using rows = Rows;

inline constexpr Wrap wrap{};

namespace alignment {

inline constexpr HorizontalAlign leading{
    components::HorizontalAlignment::leading};
inline constexpr HorizontalAlign trailing{
    components::HorizontalAlignment::trailing};
inline constexpr VerticalAlign top{components::VerticalAlignment::top};
inline constexpr VerticalAlign bottom{components::VerticalAlignment::bottom};
inline constexpr VerticalAlign first_text_baseline{
    components::VerticalAlignment::first_text_baseline};
inline constexpr VerticalAlign last_text_baseline{
    components::VerticalAlignment::last_text_baseline};
using guide = GuideAlign;
inline constexpr CenterAlign center{};
inline constexpr CornerAlign top_leading{components::Alignment::top_leading};
inline constexpr CornerAlign top_trailing{components::Alignment::top_trailing};
inline constexpr CornerAlign bottom_leading{
    components::Alignment::bottom_leading};
inline constexpr CornerAlign bottom_trailing{
    components::Alignment::bottom_trailing};

}

namespace justify {

inline constexpr Justify start{components::Distribution::start};
inline constexpr Justify center{components::Distribution::center};
inline constexpr Justify end{components::Distribution::end};
inline constexpr Justify space_between{components::Distribution::space_between};
inline constexpr Justify space_around{components::Distribution::space_around};
inline constexpr Justify space_evenly{components::Distribution::space_evenly};

}

}
