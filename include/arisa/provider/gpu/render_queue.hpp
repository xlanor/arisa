#pragma once

#include <arisa/support/scoped_callback.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::provider {

class RenderQueue {
  class State;

 public:
  class Lease {
   public:
    Lease() = default;
    Lease(Lease&& other) noexcept;
    auto operator=(Lease&& other) noexcept -> Lease&;
    Lease(const Lease&) = delete;
    auto operator=(const Lease&) -> Lease& = delete;
    ~Lease();

    auto held() const noexcept -> bool { return state_ != nullptr; }

    void release() noexcept;

   private:
    friend class RenderQueue::State;

    Lease(std::shared_ptr<State> state, std::uint8_t holder) noexcept
        : state_(std::move(state)), holder_(holder) {}

    std::shared_ptr<State> state_;
    std::uint8_t holder_{0};
  };

  struct Counters {
    std::uint64_t leases{0};
    std::uint64_t parks{0};
    std::uint64_t submissions{0};
    std::uint64_t lease_waits{0};
    std::uint64_t submission_waits{0};
    std::uint64_t overlaps{0};

    friend constexpr auto operator==(const Counters&, const Counters&)
        -> bool = default;
  };

  RenderQueue();

  [[nodiscard]] auto lease() -> Lease;

  [[nodiscard]] auto try_lease() -> std::optional<Lease>;

  [[nodiscard]] auto park() -> Lease;

  [[nodiscard]] auto submit() -> std::optional<Lease>;

  [[nodiscard]] auto withdraw() -> support::ScopedCallback;

  auto parked() const -> bool;

  auto counters() const -> Counters;

 private:
  std::shared_ptr<State> state_;
};

}
