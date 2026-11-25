#include <frame_report.hpp>

#include <arisa/provider/diagnostics/heap_meter.hpp>
#include <arisa/runtime/frame_breakdown.hpp>

#include <switch.h>

#include <algorithm>
#include <format>
#include <span>
#include <utility>

namespace arisa::nx {

FrameReport::FrameReport(const NxlinkLog& log, std::string_view name,
                         Describe describe)
    : log_(log), name_(name), describe_(std::move(describe)) {}

auto FrameReport::milliseconds(std::chrono::nanoseconds value) noexcept
    -> double {
  return std::chrono::duration<double, std::milli>(value).count();
}

void FrameReport::note(const motion::UnhintedAnimation& animation) noexcept {
  const std::span<Cause> known = std::span{causes_}.first(cause_count_);
  if (const auto found = std::ranges::find(known, animation, &Cause::animation);
      found != known.end()) {
    ++found->frames;
    return;
  }
  if (cause_count_ < causes_.size()) {
    causes_[cause_count_++] = Cause{.animation = animation, .frames = 1};
    return;
  }
  ++other_causes_;
}

auto FrameReport::weight(const Sample& sample) noexcept
    -> std::chrono::nanoseconds {
  return sample.frame.has_value() ? sample.frame->wall
                                  : sample.cpu + sample.present;
}

auto FrameReport::breakdown(const Sample& sample) const -> std::string {
  if (!sample.frame.has_value()) {
    return std::format("cpu {:.3f} ms present {:.3f} ms",
                       milliseconds(sample.cpu), milliseconds(sample.present));
  }
  return std::format(
      "{} | focus {}",
      runtime::FrameBreakdown::describe(*sample.frame, sample.allocations),
      !sample.focus.has_value() ? std::string{"none"}
      : describe_               ? describe_(*sample.focus)
                                : std::format("node {}", *sample.focus));
}

auto FrameReport::threads(std::uint64_t elapsed_ns) -> std::string {
  std::vector<provider::ThreadCpu> current = provider::ThreadCpuMeter::sample();
  const std::vector<provider::ThreadCpu> spent =
      provider::ThreadCpuMeter::since(threads_, current);
  threads_ = std::move(current);
  std::string line = std::format("{}: thread cpu", name_);
  const double window =
      static_cast<double>(std::max<std::uint64_t>(elapsed_ns, 1));
  for (const provider::ThreadCpu& thread : spent) {
    line +=
        std::format(" | {} {:.1f}%", thread.name,
                    100.0 * static_cast<double>(thread.cpu.count()) / window);
  }
  return line;
}

void FrameReport::record(const Sample& sample) {
  const std::uint64_t now = armGetSystemTick();
  if (first_) {
    first_ = false;
    reported_missed_ = sample.missed_vsyncs;
    log_.get().emit(std::format(
        "{}: first frame (cpu {:.3f} ms, layout {:.3f} ms, present {:.3f} ms)",
        name_, milliseconds(sample.cpu), milliseconds(sample.layout),
        milliseconds(sample.present)));
  }
  if (!window_start_.has_value()) {
    window_start_ = now;
    threads_ = provider::ThreadCpuMeter::sample();
  }
  ++frames_;
  if (!worst_.has_value() || weight(sample) > weight(*worst_)) {
    worst_ = sample;
  }
  if (weight(sample) > long_frame) {
    ++long_frames_;
    log_.get().emit(std::format("{}: long frame at {:.3f} s | {}", name_,
                                milliseconds(sample.now) / 1000.0,
                                breakdown(sample)));
  }
  layouts_ += sample.laid_out ? 1 : 0;
  layout_peak_ = std::max(layout_peak_, sample.layout);
  input_total_ += sample.input;
  input_peak_ = std::max(input_peak_, sample.input);
  paced_ += sample.paced ? 1 : 0;
  animated_ += sample.animating ? 1 : 0;
  culled_peak_ = std::max(culled_peak_, sample.culled);
  occluded_peak_ = std::max(occluded_peak_, sample.occluded);
  frame_rate_ = sample.frame_rate;
  due_ = sample.due;
  now_ = sample.now;
  backdrops_ = std::max(backdrops_, sample.backdrops);
  blurs_ += sample.blurs;
  blur_hits_ += sample.blur_hits;
  effect_passes_ += sample.effect_passes;
  layer_renders_ += sample.layer_renders;
  if (sample.frame.has_value() && sample.frame->encode_misses > 0) {
    log_.get().emit(std::format(
        "{}: frame error: {} textures missing at encode, last texture {} as "
        "{}",
        name_, sample.frame->encode_misses, sample.frame->encode_missed_texture,
        sample.frame->encode_missed_target ? "pass target"
                                           : "sampled texture"));
  }
  if (sample.frame.has_value()) {
    targets_created_ += sample.frame->targets_created;
    targets_reused_ += sample.frame->targets_reused;
    targets_released_ += sample.frame->targets_released;
    targets_evicted_ += sample.frame->targets_evicted;
    targets_refused_ += sample.frame->targets_refused;
  }
  if (sample.unhinted.has_value()) {
    ++unhinted_frames_;
    note(*sample.unhinted);
  }
  if (sample.effects_gpu.has_value()) {
    ++effect_frames_;
    effects_total_ += *sample.effects_gpu;
    effects_peak_ = std::max(effects_peak_, *sample.effects_gpu);
  }
  if (sample.interface_gpu.has_value()) {
    ++interface_frames_;
    interface_total_ += *sample.interface_gpu;
    interface_peak_ = std::max(interface_peak_, *sample.interface_gpu);
  }
  if (sample.hooks_gpu.has_value()) {
    ++hook_frames_;
    hooks_total_ += *sample.hooks_gpu;
    hooks_peak_ = std::max(hooks_peak_, *sample.hooks_gpu);
  }
  if (sample.presented) {
    ++presented_;
    cpu_total_ += sample.cpu;
    cpu_peak_ = std::max(cpu_peak_, sample.cpu);
    present_total_ += sample.present;
    present_peak_ = std::max(present_peak_, sample.present);
  }
  if (armTicksToNs(now - *window_start_) >= 1'000'000'000ULL) {
    flush(now, sample.missed_vsyncs);
  }
}

void FrameReport::flush(std::uint64_t now, std::uint64_t missed_vsyncs) {
  const auto shown =
      static_cast<std::int64_t>(std::max<std::uint64_t>(presented_, 1));
  log_.get().emit(std::format(
      "{}: {} frames, {} presented, {} idle | cpu avg {:.3f} ms max {:.3f} ms "
      "| layout max {:.3f} ms ({} passes) | input avg {:.3f} ms max {:.3f} ms "
      "| present avg {:.3f} ms max {:.3f} ms | missed vsyncs +{} (total {})",
      name_, frames_, presented_, frames_ - presented_,
      milliseconds(cpu_total_ / shown), milliseconds(cpu_peak_),
      milliseconds(layout_peak_), layouts_,
      milliseconds(input_total_ / static_cast<std::int64_t>(
                                      std::max<std::uint64_t>(frames_, 1))),
      milliseconds(input_peak_), milliseconds(present_total_ / shown),
      milliseconds(present_peak_), missed_vsyncs - reported_missed_,
      missed_vsyncs));
  const auto heap = provider::HeapMeter::sample();
  log_.get().emit(std::format(
      "{}: {} paced skips, {} animating | culled max {}, occluded max {} | "
      "rate {} | due {} now {} ms | heap {} bytes",
      name_, paced_, animated_, culled_peak_, occluded_peak_,
      frame_rate_.has_value() ? std::format("{:.0f}", *frame_rate_)
                              : std::string{"full"},
      due_.has_value() ? std::format("{:.3f}", milliseconds(*due_))
                       : std::string{"none"},
      milliseconds(now_), heap.has_value() ? heap->bytes : 0));
  const auto effect_count =
      static_cast<std::int64_t>(std::max<std::uint64_t>(effect_frames_, 1));
  const auto interface_count =
      static_cast<std::int64_t>(std::max<std::uint64_t>(interface_frames_, 1));
  const auto hook_count =
      static_cast<std::int64_t>(std::max<std::uint64_t>(hook_frames_, 1));
  log_.get().emit(std::format(
      "{}: gpu effects avg {:.3f} ms max {:.3f} ms ({} frames) | gpu ui avg "
      "{:.3f} ms max {:.3f} ms | gpu hooks avg {:.3f} ms max {:.3f} ms ({} "
      "frames) | backdrops max {} | blurs {} cached {} | effect passes {} "
      "layer renders {} | targets +{} reused {} released {} evicted {} "
      "refused {}",
      name_, milliseconds(effects_total_ / effect_count),
      milliseconds(effects_peak_), effect_frames_,
      milliseconds(interface_total_ / interface_count),
      milliseconds(interface_peak_), milliseconds(hooks_total_ / hook_count),
      milliseconds(hooks_peak_), hook_frames_, backdrops_, blurs_, blur_hits_,
      effect_passes_, layer_renders_, targets_created_, targets_reused_,
      targets_released_, targets_evicted_, targets_refused_));
  if (unhinted_frames_ > 0) {
    std::string line =
        std::format("{}: unhinted {} frames", name_, unhinted_frames_);
    for (const Cause& cause : std::span{causes_}.first(cause_count_)) {
      line += std::format(
          " | {} {} node {} ({} frames){}",
          motion::UnhintedAnimation::name(cause.animation.route),
          cause.animation.channel, cause.animation.node, cause.frames,
          describe_ ? std::format(" {}", describe_(cause.animation.node))
                    : std::string{});
    }
    if (other_causes_ > 0) {
      line += std::format(" | {} frames from other causes", other_causes_);
    }
    log_.get().emit(line);
  }
  unhinted_frames_ = 0;
  cause_count_ = 0;
  other_causes_ = 0;
  if (worst_.has_value() && worst_->frame.has_value()) {
    log_.get().emit(std::format("{}: worst frame ({} over {} ms) | {}", name_,
                                long_frames_, long_frame.count(),
                                breakdown(*worst_)));
  }
  worst_.reset();
  long_frames_ = 0;
  log_.get().emit(threads(armTicksToNs(now - *window_start_)));
  effects_total_ = std::chrono::nanoseconds{0};
  effects_peak_ = std::chrono::nanoseconds{0};
  interface_total_ = std::chrono::nanoseconds{0};
  interface_peak_ = std::chrono::nanoseconds{0};
  effect_frames_ = 0;
  interface_frames_ = 0;
  hooks_total_ = std::chrono::nanoseconds{0};
  hooks_peak_ = std::chrono::nanoseconds{0};
  hook_frames_ = 0;
  blurs_ = 0;
  blur_hits_ = 0;
  effect_passes_ = 0;
  layer_renders_ = 0;
  targets_created_ = 0;
  targets_reused_ = 0;
  targets_released_ = 0;
  targets_evicted_ = 0;
  targets_refused_ = 0;
  backdrops_ = 0;
  reported_missed_ = missed_vsyncs;
  paced_ = 0;
  animated_ = 0;
  culled_peak_ = 0;
  occluded_peak_ = 0;
  window_start_ = now;
  frames_ = 0;
  presented_ = 0;
  layouts_ = 0;
  cpu_total_ = std::chrono::nanoseconds{0};
  cpu_peak_ = std::chrono::nanoseconds{0};
  layout_peak_ = std::chrono::nanoseconds{0};
  input_total_ = std::chrono::nanoseconds{0};
  input_peak_ = std::chrono::nanoseconds{0};
  present_total_ = std::chrono::nanoseconds{0};
  present_peak_ = std::chrono::nanoseconds{0};
}

}
