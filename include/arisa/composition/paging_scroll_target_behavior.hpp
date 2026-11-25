#pragma once

#include <arisa/composition/scroll_target.hpp>
#include <arisa/composition/scroll_target_behavior_properties.hpp>

namespace arisa::composition {

class PagingScrollTargetBehavior {
 public:
  PagingScrollTargetBehavior() noexcept = default;

  void update_target(ScrollTarget& target,
                     const ScrollTargetContext& context) const;

  auto properties(const ScrollTargetBehaviorPropertiesContext& context) const
      -> ScrollTargetBehaviorProperties;

  friend constexpr auto operator==(const PagingScrollTargetBehavior&,
                                   const PagingScrollTargetBehavior&)
      -> bool = default;

 private:
  static auto paged(float original, float starting, float page) -> float;
};

}
