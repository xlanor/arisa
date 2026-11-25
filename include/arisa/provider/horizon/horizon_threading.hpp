#pragma once

#include <arisa/provider/standard/std_threading.hpp>
#include <arisa/provider/threading/provider.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace arisa::provider::horizon {

class HorizonThreadingProvider final : public ThreadingProvider {
 public:
  static constexpr std::uint64_t fallback_cores = 0b0111;

  explicit HorizonThreadingProvider(ThreadPlacementPolicy policy = {});

  [[nodiscard]] auto start(ThreadSpec spec, ThreadBody body)
      -> std::expected<Thread, ProviderError> override;

  auto placement() const noexcept -> const ThreadPlacementPolicy& override;

  auto current_thread() const noexcept -> ThreadId override;

  void adopt_current_thread(ThreadRole role) noexcept override;

  static auto usable_cores(CoreMask requested) noexcept -> std::uint64_t;

  auto orphans() const -> std::size_t;

 private:
  static void place_current_thread(std::string_view name,
                                   const ThreadPlacement& placement) noexcept;
  static auto process_cores() noexcept -> std::uint64_t;

  standard::StdThreadingProvider threads_;
};

}
