#pragma once

#include <nxlink_log.hpp>

#include <arisa/motion/unhinted_animation.hpp>
#include <arisa/provider/diagnostics/thread_cpu_meter.hpp>
#include <arisa/runtime/frame_timings.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::nx {

class FrameReport {
 public:
  struct Sample {
    std::chrono::nanoseconds cpu{0};
    std::chrono::nanoseconds layout{0};
    std::chrono::nanoseconds input{0};
    std::chrono::nanoseconds present{0};
    bool laid_out{false};
    bool presented{true};
    std::uint64_t missed_vsyncs{0};
    bool paced{false};
    bool animating{false};
    std::uint32_t culled{0};
    std::uint32_t occluded{0};
    std::optional<double> frame_rate{};
    std::optional<std::chrono::nanoseconds> due{};
    std::chrono::nanoseconds now{0};
    std::optional<std::chrono::nanoseconds> effects_gpu{};
    std::optional<std::chrono::nanoseconds> interface_gpu{};
    std::optional<std::chrono::nanoseconds> hooks_gpu{};
    std::uint32_t backdrops{0};
    std::uint32_t blurs{0};
    std::uint32_t blur_hits{0};
    std::uint32_t effect_passes{0};
    std::uint32_t layer_renders{0};
    std::optional<motion::UnhintedAnimation> unhinted{};
    std::optional<runtime::FrameTimings> frame{};
    std::optional<std::uint64_t> allocations{};
    std::optional<std::uint64_t> focus{};
  };

  static constexpr std::chrono::milliseconds long_frame{50};
  static constexpr std::size_t unhinted_causes = 4;

  using Describe = std::move_only_function<std::string(std::uint64_t) const>;

  FrameReport(const NxlinkLog& log, std::string_view name,
              Describe describe = {});

  void record(const Sample& sample);

 private:
  struct Cause {
    motion::UnhintedAnimation animation{};
    std::uint64_t frames{0};
  };

  void note(const motion::UnhintedAnimation& animation) noexcept;

  void flush(std::uint64_t now, std::uint64_t missed_vsyncs);

  static auto milliseconds(std::chrono::nanoseconds value) noexcept -> double;

  static auto weight(const Sample& sample) noexcept -> std::chrono::nanoseconds;

  auto breakdown(const Sample& sample) const -> std::string;

  auto threads(std::uint64_t elapsed_ns) -> std::string;

  std::reference_wrapper<const NxlinkLog> log_;
  std::string name_;
  std::optional<std::uint64_t> window_start_;
  std::uint64_t frames_{0};
  std::uint64_t presented_{0};
  std::uint64_t layouts_{0};
  std::chrono::nanoseconds cpu_total_{0};
  std::chrono::nanoseconds cpu_peak_{0};
  std::chrono::nanoseconds layout_peak_{0};
  std::chrono::nanoseconds input_total_{0};
  std::chrono::nanoseconds input_peak_{0};
  std::chrono::nanoseconds present_total_{0};
  std::chrono::nanoseconds present_peak_{0};
  std::uint64_t reported_missed_{0};
  std::uint64_t paced_{0};
  std::uint64_t animated_{0};
  std::uint32_t culled_peak_{0};
  std::uint32_t occluded_peak_{0};
  std::optional<double> frame_rate_;
  std::optional<std::chrono::nanoseconds> due_;
  std::chrono::nanoseconds now_{0};
  std::chrono::nanoseconds effects_total_{0};
  std::chrono::nanoseconds effects_peak_{0};
  std::chrono::nanoseconds interface_total_{0};
  std::chrono::nanoseconds interface_peak_{0};
  std::uint64_t effect_frames_{0};
  std::uint64_t interface_frames_{0};
  std::chrono::nanoseconds hooks_total_{0};
  std::chrono::nanoseconds hooks_peak_{0};
  std::uint64_t hook_frames_{0};
  std::uint64_t blurs_{0};
  std::uint64_t blur_hits_{0};
  std::uint64_t effect_passes_{0};
  std::uint64_t layer_renders_{0};
  std::uint64_t targets_created_{0};
  std::uint64_t targets_reused_{0};
  std::uint64_t targets_released_{0};
  std::uint64_t targets_evicted_{0};
  std::uint64_t targets_refused_{0};
  std::uint32_t backdrops_{0};
  bool first_{true};
  std::vector<provider::ThreadCpu> threads_;
  Describe describe_;
  std::uint64_t unhinted_frames_{0};
  std::array<Cause, unhinted_causes> causes_{};
  std::size_t cause_count_{0};
  std::uint64_t other_causes_{0};
  std::optional<Sample> worst_;
  std::uint64_t long_frames_{0};
};

}
