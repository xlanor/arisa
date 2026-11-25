#pragma once

#include <arisa/provider/gpu/frame_info.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>

namespace arisa::examples {

class Animation {
 public:
  enum class Pattern : std::uint8_t {
    plasma,
    ripples,
    tunnel,
  };

  struct Uniforms {
    float time{0.0F};
    float pattern{0.0F};
    float aspect{1.0F};
    float speed{1.0F};
    float plane{0.0F};
    float matrix{0.0F};
    float range{0.0F};
    float luma_width{1.0F};
  };

  static constexpr std::chrono::milliseconds longest_step{100};

  auto advance(Pattern pattern, const provider::FrameInfo& info) -> Uniforms;

  auto latest(Pattern pattern, std::uint32_t width, std::uint32_t height) const
      -> Uniforms;

  void play(bool playing) noexcept;

  auto playing() const noexcept -> bool;

 private:
  static auto uniforms(Pattern pattern, double time, std::uint32_t width,
                       std::uint32_t height) noexcept -> Uniforms;

  std::atomic<bool> playing_{true};
  mutable std::mutex mutex_;
  std::optional<std::chrono::nanoseconds> last_;
  double time_{0.0};
};

}
