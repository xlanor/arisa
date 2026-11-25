#include <arisa/provider/threading/thread_placement_policy.hpp>

#include <cstdint>
#include <utility>

namespace arisa::provider {

ThreadPlacementPolicy::ThreadPlacementPolicy() noexcept
    : placements_{
          ThreadPlacement{CoreMask{0b0001}, ThreadPriority::of<0x2C>(),
                          StackSize{thread_stack}},
          ThreadPlacement{CoreMask{0b0110}, ThreadPriority::of<0x2D>(),
                          StackSize{thread_stack}},
          ThreadPlacement{CoreMask{0b0100}, ThreadPriority::of<0x2E>(),
                          StackSize{thread_stack}},
          ThreadPlacement{CoreMask{0b0010}, ThreadPriority::of<0x2C>(),
                          StackSize{thread_stack}},
          ThreadPlacement{CoreMask{0b0111}, ThreadPriority::of<0x2C>(),
                          StackSize{thread_stack}},
          ThreadPlacement{CoreMask{0b0010}, ThreadPriority::of<0x2C>(),
                          StackSize{thread_stack}},
      } {}

auto ThreadPlacementPolicy::placement(ThreadRole role) const noexcept
    -> const ThreadPlacement& {
  return placements_[index(role)];
}

auto ThreadPlacementPolicy::with(ThreadRole role,
                                 ThreadPlacement placement) const noexcept
    -> ThreadPlacementPolicy {
  ThreadPlacementPolicy updated = *this;
  updated.placements_[index(role)] = placement;
  return updated;
}

auto ThreadPlacementPolicy::restricted_to(CoreMask available) const noexcept
    -> ThreadPlacementPolicy {
  if (available.value() == 0) {
    return *this;
  }
  ThreadPlacementPolicy restricted = *this;
  for (ThreadPlacement& placement : restricted.placements_) {
    const std::uint64_t allowed = placement.cores().value() & available.value();
    placement.cores_ = CoreMask{allowed != 0 ? allowed : available.value()};
  }
  return restricted;
}

auto ThreadPlacementPolicy::index(ThreadRole role) noexcept -> std::size_t {
  return std::to_underlying(role);
}

}
