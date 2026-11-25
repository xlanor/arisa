#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/support/bounded_value.hpp>
#include <arisa/support/strong_value.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>

namespace arisa::provider {

struct CoreMaskTag;
struct ThreadPriorityTag;
struct StackSizeTag;

using CoreMask = support::StrongValue<std::uint64_t, CoreMaskTag>;
using ThreadPriority =
    support::BoundedValue<std::int32_t, 0, 63, ThreadPriorityTag>;
using StackSize = support::StrongValue<std::size_t, StackSizeTag>;

class ThreadPlacement {
 public:
  static constexpr std::size_t stack_alignment = 0x1000;
  static constexpr std::size_t minimum_stack = 0x4000;

  [[nodiscard]] static auto create(CoreMask cores, ThreadPriority priority,
                                   StackSize stack)
      -> std::expected<ThreadPlacement, ProviderError>;

  auto cores() const noexcept -> CoreMask { return cores_; }
  auto priority() const noexcept -> ThreadPriority { return priority_; }
  auto stack() const noexcept -> StackSize { return stack_; }

  friend auto operator==(const ThreadPlacement&, const ThreadPlacement&)
      -> bool = default;

 private:
  friend class ThreadPlacementPolicy;

  constexpr ThreadPlacement(CoreMask cores, ThreadPriority priority,
                            StackSize stack) noexcept
      : cores_(cores), priority_(priority), stack_(stack) {}

  CoreMask cores_;
  ThreadPriority priority_;
  StackSize stack_;
};

}
