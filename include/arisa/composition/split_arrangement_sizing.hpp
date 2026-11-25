#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/layout_value_key.hpp>

#include <array>
#include <cstddef>
#include <optional>

namespace arisa::composition {

class SplitArrangementSizing {
 public:
  struct Bounds {
    std::optional<float> minimum{};
    std::optional<float> ideal{};
    std::optional<float> maximum{};

    constexpr auto declared() const noexcept -> bool {
      return minimum.has_value() || ideal.has_value() || maximum.has_value();
    }

    friend constexpr auto operator==(const Bounds&, const Bounds&)
        -> bool = default;
  };

  struct Ratios {
    std::optional<float> min_horizontal{};
    std::optional<float> ideal_horizontal{};
    std::optional<float> max_horizontal{};
    std::optional<float> min_vertical{};
    std::optional<float> ideal_vertical{};
    std::optional<float> max_vertical{};
  };

  struct Sizes {
    std::optional<float> min_width{};
    std::optional<float> ideal_width{};
    std::optional<float> max_width{};
    std::optional<float> min_height{};
    std::optional<float> ideal_height{};
    std::optional<float> max_height{};
  };

  static const LayoutValueKey<SplitArrangementSizing> key;

  constexpr SplitArrangementSizing() noexcept = default;

  constexpr auto ratio(Axis axis) const noexcept -> const Bounds& {
    return ratio_[slot(axis)];
  }

  constexpr auto size(Axis axis) const noexcept -> const Bounds& {
    return size_[slot(axis)];
  }

  constexpr auto fixed(Axis axis) const noexcept -> bool {
    return fixed_[slot(axis)];
  }

  constexpr auto with_ratio(std::optional<float> value) const noexcept
      -> SplitArrangementSizing {
    return with_ratios(
        Ratios{.ideal_horizontal = value, .ideal_vertical = value});
  }

  constexpr auto with_ratios(Ratios ratios) const noexcept
      -> SplitArrangementSizing {
    SplitArrangementSizing changed = *this;
    changed.ratio_ = {Bounds{.minimum = ratios.min_horizontal,
                             .ideal = ratios.ideal_horizontal,
                             .maximum = ratios.max_horizontal},
                      Bounds{.minimum = ratios.min_vertical,
                             .ideal = ratios.ideal_vertical,
                             .maximum = ratios.max_vertical}};
    return changed;
  }

  constexpr auto with_sizes(Sizes sizes) const noexcept
      -> SplitArrangementSizing {
    SplitArrangementSizing changed = *this;
    changed.size_ = {Bounds{.minimum = sizes.min_width,
                            .ideal = sizes.ideal_width,
                            .maximum = sizes.max_width},
                     Bounds{.minimum = sizes.min_height,
                            .ideal = sizes.ideal_height,
                            .maximum = sizes.max_height}};
    return changed;
  }

  constexpr auto with_fixed(bool horizontal, bool vertical) const noexcept
      -> SplitArrangementSizing {
    SplitArrangementSizing changed = *this;
    changed.fixed_ = {horizontal, vertical};
    return changed;
  }

  friend constexpr auto operator==(const SplitArrangementSizing&,
                                   const SplitArrangementSizing&)
      -> bool = default;

 private:
  static constexpr auto slot(Axis axis) noexcept -> std::size_t {
    return axis == Axis::horizontal ? 0U : 1U;
  }

  std::array<Bounds, 2> ratio_{};
  std::array<Bounds, 2> size_{};
  std::array<bool, 2> fixed_{};
};

inline constexpr LayoutValueKey<SplitArrangementSizing>
    SplitArrangementSizing::key{"split_arrangement_sizing",
                                SplitArrangementSizing{}};

}
