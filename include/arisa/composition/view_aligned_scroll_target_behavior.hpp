#pragma once

#include <arisa/composition/scroll_target.hpp>
#include <arisa/composition/scroll_target_behavior_properties.hpp>
#include <arisa/motion/unit_point.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace arisa::composition {

class ViewAlignedScrollTargetBehavior {
 public:
  enum class LimitBehavior : std::uint8_t {
    automatic,
    always,
    always_by_few,
    always_by_one,
    never,
  };

  static constexpr double fling_speed = 40.0;
  static constexpr std::size_t few_views = 3;

  explicit ViewAlignedScrollTargetBehavior(
      LimitBehavior limit = LimitBehavior::automatic) noexcept
      : limit_(limit) {}

  ViewAlignedScrollTargetBehavior(
      LimitBehavior limit, std::optional<motion::UnitPoint> alignment) noexcept
      : limit_(limit), anchor_(alignment) {}

  explicit ViewAlignedScrollTargetBehavior(
      std::optional<motion::UnitPoint> alignment) noexcept
      : anchor_(alignment) {}

  auto limit_behavior() const noexcept -> LimitBehavior { return limit_; }

  auto anchor() const noexcept -> std::optional<motion::UnitPoint> {
    return anchor_;
  }

  auto resolved_limit(bool horizontally_compact) const noexcept
      -> LimitBehavior;

  void update_target(ScrollTarget& target,
                     const ScrollTargetContext& context) const;

  auto properties(const ScrollTargetBehaviorPropertiesContext& context) const
      -> ScrollTargetBehaviorProperties;

  friend constexpr auto operator==(const ViewAlignedScrollTargetBehavior&,
                                   const ViewAlignedScrollTargetBehavior&)
      -> bool = default;

 private:
  auto aligned(const ScrollTargetContext& context, std::size_t index,
               bool vertical) const -> double;

  auto nearest(const ScrollTargetContext& context, double position,
               bool vertical) const -> std::size_t;

  LimitBehavior limit_{LimitBehavior::automatic};
  std::optional<motion::UnitPoint> anchor_;
};

}
