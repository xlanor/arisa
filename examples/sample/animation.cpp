#include <animation.hpp>

#include <algorithm>
#include <utility>

namespace arisa::examples {

auto Animation::advance(Pattern pattern, const provider::FrameInfo& info)
    -> Uniforms {
  const std::scoped_lock lock{mutex_};
  if (last_.has_value() && playing()) {
    const std::chrono::nanoseconds step =
        std::clamp(info.predicted_present - *last_, std::chrono::nanoseconds{0},
                   std::chrono::nanoseconds{longest_step});
    time_ += std::chrono::duration<double>(step).count();
  }
  last_ =
      std::max(last_.value_or(info.predicted_present), info.predicted_present);
  return uniforms(pattern, time_, info.extent.width.value(),
                  info.extent.height.value());
}

auto Animation::latest(Pattern pattern, std::uint32_t width,
                       std::uint32_t height) const -> Uniforms {
  const std::scoped_lock lock{mutex_};
  return uniforms(pattern, time_, width, height);
}

void Animation::play(bool playing) noexcept {
  playing_.store(playing, std::memory_order_relaxed);
}

auto Animation::playing() const noexcept -> bool {
  return playing_.load(std::memory_order_relaxed);
}

auto Animation::uniforms(Pattern pattern, double time, std::uint32_t width,
                         std::uint32_t height) noexcept -> Uniforms {
  return Uniforms{.time = static_cast<float>(time),
                  .pattern = static_cast<float>(std::to_underlying(pattern)),
                  .aspect = static_cast<float>(width) /
                            static_cast<float>(std::max(height, 1U))};
}

}
