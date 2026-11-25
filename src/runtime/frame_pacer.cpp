#include <runtime/frame_pacer.hpp>

#include <algorithm>

namespace arisa::runtime {

auto FramePacer::rate(std::optional<double> interface, bool animating,
                      const provider::FrameSchedule::Demand& demand) noexcept
    -> std::optional<double> {
  if (!demand.continuous) {
    return interface;
  }
  if (!animating) {
    return demand.frame_rate;
  }
  if (!interface.has_value() || !demand.frame_rate.has_value()) {
    return std::nullopt;
  }
  return std::max(*interface, *demand.frame_rate);
}

auto FramePacer::swap_interval(std::optional<double> rate, bool driven) noexcept
    -> std::uint32_t {
  return !driven && rate.has_value() && *rate > 0.0 && *rate <= half_rate ? 2U
                                                                          : 1U;
}

void FramePacer::presented(std::optional<std::chrono::nanoseconds> displayed,
                           std::uint32_t interval) noexcept {
  if (displayed.has_value()) {
    anchor_ = displayed;
  }
  interval_ = std::max<std::uint32_t>(interval, 1);
}

void FramePacer::built(std::chrono::nanoseconds cost) noexcept {
  costs_.at(next_cost_) = std::max(cost, std::chrono::nanoseconds{0});
  next_cost_ = (next_cost_ + 1) % history;
}

auto FramePacer::lead() const noexcept -> std::chrono::nanoseconds {
  return *std::ranges::max_element(costs_) + margin;
}

auto FramePacer::active(std::chrono::nanoseconds now,
                        bool continuous) const noexcept -> bool {
  return enabled_ && continuous && anchor_.has_value() &&
         now - *anchor_ <= display_period * (stale_periods * interval_);
}

auto FramePacer::target(std::chrono::nanoseconds now) const noexcept
    -> std::chrono::nanoseconds {
  const std::chrono::nanoseconds anchor =
      anchor_.value_or(std::chrono::nanoseconds{0});
  const std::chrono::nanoseconds earliest = now + lead();
  std::chrono::nanoseconds vsync = anchor + (display_period * interval_);
  if (vsync < earliest) {
    const std::int64_t periods =
        (earliest - anchor + display_period - std::chrono::nanoseconds{1}) /
        display_period;
    vsync = anchor + (display_period * periods);
  }
  return vsync;
}

auto FramePacer::next_build(std::chrono::nanoseconds now) const noexcept
    -> std::chrono::nanoseconds {
  if (!anchor_.has_value()) {
    return now;
  }
  return std::max(now, target(now) - lead());
}

auto FramePacer::predicted_present(std::chrono::nanoseconds now) const noexcept
    -> std::chrono::nanoseconds {
  if (!anchor_.has_value()) {
    return now + display_period;
  }
  return target(now);
}

auto FramePacer::skip_until(std::optional<double> rate,
                            std::chrono::nanoseconds started) noexcept
    -> std::optional<std::chrono::nanoseconds> {
  if (!rate.has_value() || *rate >= full_rate) {
    return std::nullopt;
  }
  const auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(1.0 / *rate));
  return started + period - software_slack;
}

}
