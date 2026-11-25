#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/frame_schedule.hpp>
#include <arisa/provider/gpu/render_queue.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>

namespace arisa::provider {

class GpuTakeover {
  class State;

 public:
  struct Counters {
    std::uint64_t granted{0};
    std::uint64_t returned{0};
    std::uint64_t refused{0};

    friend constexpr auto operator==(const Counters&, const Counters&)
        -> bool = default;
  };

  using HandBack = std::move_only_function<support::ScopedCallback()>;

  class Seat {
   public:
    Seat();

    [[nodiscard]] auto claim(RenderQueue& queue,
                             std::weak_ptr<FrameSchedule> wake,
                             HandBack prepare)
        -> std::expected<GpuTakeover, ProviderError>;

    auto held() const -> bool;

    auto counters() const -> Counters;

   private:
    std::shared_ptr<State> state_;
  };

  GpuTakeover() = default;
  GpuTakeover(GpuTakeover&& other) noexcept = default;
  auto operator=(GpuTakeover&& other) noexcept -> GpuTakeover&;
  GpuTakeover(const GpuTakeover&) = delete;
  auto operator=(const GpuTakeover&) -> GpuTakeover& = delete;
  ~GpuTakeover();

  auto held() const noexcept -> bool { return state_ != nullptr; }

  void release() noexcept;

 private:
  GpuTakeover(std::shared_ptr<State> state, RenderQueue::Lease park,
              support::ScopedCallback refusing,
              std::weak_ptr<FrameSchedule> wake) noexcept;

  std::shared_ptr<State> state_;
  RenderQueue::Lease park_;
  support::ScopedCallback refusing_;
  std::weak_ptr<FrameSchedule> wake_;
  support::ScopedCallback hand_back_;
};

}
