#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/haptics/rumble_motor.hpp>
#include <arisa/provider/haptics/rumble_shape.hpp>
#include <arisa/provider/threading/provider.hpp>

#include <cstdint>
#include <expected>
#include <memory>

namespace arisa::provider {

class RumbleQueue {
 public:
  struct Counters {
    std::uint64_t requested{0};
    std::uint64_t contended{0};
    std::uint64_t started{0};
    std::uint64_t interrupted{0};

    friend constexpr auto operator==(const Counters&, const Counters&)
        -> bool = default;
  };

  [[nodiscard]] static auto create(ThreadingProvider& threading,
                                   std::unique_ptr<RumbleMotor> motor)
      -> std::expected<RumbleQueue, ProviderError>;

  RumbleQueue(const RumbleQueue&) = delete;
  auto operator=(const RumbleQueue&) -> RumbleQueue& = delete;
  RumbleQueue(RumbleQueue&& other) noexcept;
  auto operator=(RumbleQueue&& other) noexcept -> RumbleQueue&;
  ~RumbleQueue();

  void play(const RumbleShape& shape);

  auto counters() const noexcept -> Counters;

 private:
  class State;

  RumbleQueue(std::unique_ptr<State> state, Thread thread) noexcept;

  std::unique_ptr<State> state_;
  Thread thread_;
};

}
