#pragma once

#include <arisa/provider/gpu/frame_schedule.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arisa::runtime {

class FramePacer {
 public:
  static constexpr std::chrono::nanoseconds display_period{16'666'667};
  static constexpr std::chrono::milliseconds margin{2};
  static constexpr std::chrono::milliseconds software_slack{4};
  static constexpr double full_rate = 59.0;
  static constexpr double half_rate = 30.5;
  static constexpr std::size_t history = 16;
  static constexpr std::uint32_t stale_periods = 8;

  static auto rate(std::optional<double> interface, bool animating,
                   const provider::FrameSchedule::Demand& demand) noexcept
      -> std::optional<double>;

  static auto swap_interval(std::optional<double> rate, bool driven) noexcept
      -> std::uint32_t;

  explicit FramePacer(bool enabled) noexcept : enabled_(enabled) {}

  void presented(std::optional<std::chrono::nanoseconds> displayed,
                 std::uint32_t interval) noexcept;

  void built(std::chrono::nanoseconds cost) noexcept;

  auto active(std::chrono::nanoseconds now, bool continuous) const noexcept
      -> bool;

  auto next_build(std::chrono::nanoseconds now) const noexcept
      -> std::chrono::nanoseconds;

  auto predicted_present(std::chrono::nanoseconds now) const noexcept
      -> std::chrono::nanoseconds;

  static auto skip_until(std::optional<double> rate,
                         std::chrono::nanoseconds started) noexcept
      -> std::optional<std::chrono::nanoseconds>;

  auto lead() const noexcept -> std::chrono::nanoseconds;

  auto interval() const noexcept -> std::uint32_t { return interval_; }

 private:
  auto target(std::chrono::nanoseconds now) const noexcept
      -> std::chrono::nanoseconds;

  bool enabled_;
  std::optional<std::chrono::nanoseconds> anchor_;
  std::uint32_t interval_{1};
  std::array<std::chrono::nanoseconds, history> costs_{};
  std::size_t next_cost_{0};
};

}
