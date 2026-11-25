#pragma once

#include <arisa/provider/clock/provider.hpp>
#include <arisa/provider/gpu/frame_packet.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/threading/provider.hpp>
#include <arisa/provider/threading/thread.hpp>
#include <arisa/rendering/replay_scene.hpp>
#include <arisa/runtime/application_error.hpp>
#include <arisa/runtime/captured_frame.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>

namespace arisa::runtime {

class RenderThread {
 public:
  static constexpr double default_rate = 60.0;
  static constexpr std::chrono::milliseconds slack{4};
  static constexpr std::chrono::milliseconds park_poll{1};
  static constexpr std::chrono::nanoseconds display_period{16'666'667};
  static constexpr std::size_t scene_count = 4;

  struct Handoff {
    std::chrono::nanoseconds stall{0};
    std::optional<provider::FrameReceipt> receipt{};
  };

  [[nodiscard]] static auto create(provider::ThreadingProvider& threading,
                                   provider::GpuProvider& gpu,
                                   provider::ClockProvider& clock)
      -> std::expected<RenderThread, ApplicationError>;

  RenderThread(RenderThread&& other) noexcept = default;
  auto operator=(RenderThread&& other) noexcept -> RenderThread& = default;
  RenderThread(const RenderThread&) = delete;
  auto operator=(const RenderThread&) -> RenderThread& = delete;
  ~RenderThread() = default;

  auto scene() -> rendering::ReplayScene&;

  auto replayed_since() noexcept -> bool;

  [[nodiscard]] auto submit(provider::FramePacket packet,
                            std::chrono::nanoseconds presented)
      -> std::expected<Handoff, ApplicationError>;

  void abandon() noexcept;

  void step();

  void drain();

  auto take_capture() -> std::optional<CapturedFrame>;

  auto replayed() const noexcept -> std::uint64_t;

  auto wakes() const noexcept -> std::uint64_t;

  auto submitted() const noexcept -> std::uint64_t;

  auto lockstep() const noexcept -> bool;

 private:
  class State;

  RenderThread(std::shared_ptr<State> state, provider::Thread thread) noexcept;

  std::shared_ptr<State> state_;
  provider::Thread thread_;
};

}
