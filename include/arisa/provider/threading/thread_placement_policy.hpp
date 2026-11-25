#pragma once

#include <arisa/provider/threading/thread_placement.hpp>
#include <arisa/provider/threading/thread_role.hpp>

#include <array>
#include <cstddef>

namespace arisa::provider {

class ThreadPlacementPolicy {
 public:
  static constexpr std::size_t thread_stack = 0x20000;

  ThreadPlacementPolicy() noexcept;

  auto placement(ThreadRole role) const noexcept -> const ThreadPlacement&;

  auto with(ThreadRole role, ThreadPlacement placement) const noexcept
      -> ThreadPlacementPolicy;

  auto restricted_to(CoreMask available) const noexcept
      -> ThreadPlacementPolicy;

  friend auto operator==(const ThreadPlacementPolicy&,
                         const ThreadPlacementPolicy&) -> bool = default;

 private:
  static auto index(ThreadRole role) noexcept -> std::size_t;

  std::array<ThreadPlacement, all_thread_roles.size()> placements_;
};

}
