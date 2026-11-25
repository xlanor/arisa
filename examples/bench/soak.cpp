#include <soak.hpp>

#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/diagnostics/heap_meter.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/frame_timings.hpp>

#include <gallery_fonts.hpp>

#include <algorithm>
#include <format>
#include <ranges>
#include <utility>

namespace arisa::examples {

Soak::Soak(provider::ProviderFactory& providers,
           provider::headless::ScriptedInput& input, Advance advance,
           bool real_time, Lifecycle lifecycle, Sink sink) noexcept
    : providers_(providers),
      input_(input),
      advance_(std::move(advance)),
      real_time_(real_time),
      lifecycle_(lifecycle),
      sink_(std::move(sink)),
      remote_(std::make_shared<SoakScreen::Remote>()) {}

auto Soak::run(Options options)
    -> std::expected<Report, runtime::ApplicationError> {
  provider::ProviderFactory& providers = providers_;
  auto created = runtime::Application::create(
      providers, SoakScreen{remote_},
      runtime::ApplicationOptions{
          .executors = std::nullopt,
          .fonts = GalleryFonts::load(providers.assets()),
          .atlas = {},
          .interaction = {},
          .appearance = std::nullopt,
          .launch_colour = std::nullopt});
  if (!created.has_value()) {
    return std::unexpected(created.error());
  }
  runtime::Application application = std::move(*created);
  Report report;
  cpu_.reserve(static_cast<std::size_t>(options.interval.count()) * 120);
  const provider::Instant started = providers.clock().now();
  std::chrono::seconds next = options.interval;
  std::uint32_t phase = 0;
  std::uint32_t frame = 0;
  while (true) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        providers.clock().now() - started);
    if (elapsed >= next || elapsed >= options.duration) {
      Row taken = row(elapsed);
      if (sink_) {
        sink_(taken);
      }
      report.rows.push_back(taken);
      next += options.interval;
      if (elapsed >= options.duration) {
        break;
      }
    }
    stimulate(phase, frame);
    if (!advance_()) {
      break;
    }
    const auto begun = std::chrono::steady_clock::now();
    if (auto stepped = application.frame(); !stepped.has_value()) {
      return std::unexpected(stepped.error());
    }
    const auto wall = std::chrono::steady_clock::now() - begun;
    const runtime::FrameTimings& timings = application.last_frame();
    ++frames_;
    if (timings.presented) {
      ++presented_;
      cpu_.push_back(
          std::max(std::chrono::duration_cast<std::chrono::nanoseconds>(wall) -
                       timings.blocked(),
                   std::chrono::nanoseconds{0}));
    }
    if (frames_ % heap_period == 0) {
      sample_heap();
    }
    if (real_time_ && application.waiting()) {
      application.wait();
    }
    if (++frame < phases[phase].frames) {
      continue;
    }
    frame = 0;
    if (++phase < phases.size()) {
      continue;
    }
    phase = 0;
    ++cycles_;
    if (const auto live = provider::AllocationMeter::live()) {
      report.settled_live.push_back(*live);
    }
  }
  return report;
}

void Soak::stimulate(std::uint32_t phase, std::uint32_t frame) {
  const Phase& current = phases[phase];
  if (frame == 0 && remote_->mode.has_value() &&
      remote_->mode->get() != current.mode) {
    remote_->mode->set(current.mode);
  }
  if (frame < mount_frames && current.mode != SoakScreen::Mode::gallery) {
    return;
  }
  const std::uint32_t scripted = frame - mount_frames;
  switch (current.mode) {
    case SoakScreen::Mode::gallery:
      if (phase == 0) {
        gallery(frame);
      }
      return;
    case SoakScreen::Mode::fling:
      drag(scripted % stroke_frames, (scripted / stroke_frames) % 2 == 0);
      return;
    case SoakScreen::Mode::sheet:
      if (scripted % toggle_frames == 0 && remote_->bench->sheet.has_value()) {
        remote_->bench->sheet->set((scripted / toggle_frames) % 2 == 0);
      }
      return;
    case SoakScreen::Mode::navigation:
      if (scripted % toggle_frames == 0 && remote_->bench->path.has_value()) {
        remote_->bench->path->set((scripted / toggle_frames) % 2 == 0
                                      ? std::vector<int>{1}
                                      : std::vector<int>{});
      }
      return;
  }
}

void Soak::gallery(std::uint32_t frame) {
  const std::uint32_t page = frame / page_frames;
  const std::uint32_t step = frame % page_frames;
  if (step == 0 && page > 0) {
    press(provider::Button::r);
  }
  if (step == 1 && page % theme_every == theme_every - 1) {
    press(provider::Button::y);
  }
  if (step >= drag_start && step <= drag_start + drag_steps + 1) {
    drag(step - drag_start, page % 2 == 0);
  }
}

void Soak::press(provider::Button button) {
  provider::InputFrame down;
  down.pads[0] = provider::PadState{
      .connected = true, .buttons = {button}, .left = {}, .right = {}};
  input_.get().push(std::move(down));
  provider::InputFrame up;
  up.pads[0] = provider::PadState{
      .connected = true, .buttons = {}, .left = {}, .right = {}};
  input_.get().push(std::move(up));
}

void Soak::drag(std::uint32_t step, bool down) {
  if (step > drag_steps + 1) {
    return;
  }
  const provider::SurfaceSize size = providers_.get().surface().size();
  const auto width = static_cast<float>(size.width.value());
  const auto height = static_cast<float>(size.height.value());
  const float start = height * (down ? 0.7F : 0.3F);
  const float travel = height * drag_distance * (down ? -1.0F : 1.0F);
  const float progress = static_cast<float>(std::min(step, drag_steps)) /
                         static_cast<float>(drag_steps);
  provider::InputFrame input;
  input.touches.push_back(provider::Touch{
      .id = provider::TouchId{2},
      .position = provider::PixelPoint{.x = width * 0.6F,
                                       .y = start + (travel * progress)},
      .phase = step == 0           ? provider::TouchPhase::began
               : step > drag_steps ? provider::TouchPhase::ended
                                   : provider::TouchPhase::moved});
  input_.get().push(std::move(input));
}

void Soak::sample_heap() {
  if (const auto heap = provider::HeapMeter::sample()) {
    heap_high_water_ = std::max(heap_high_water_.value_or(0), heap->bytes);
  }
}

auto Soak::percentile(std::span<const std::chrono::nanoseconds> sorted,
                      std::uint32_t share) -> std::chrono::nanoseconds {
  if (sorted.empty()) {
    return std::chrono::nanoseconds{0};
  }
  const std::size_t index =
      std::min((sorted.size() * share) / 100, sorted.size() - 1);
  return sorted[index];
}

auto Soak::row(std::chrono::seconds elapsed) -> Row {
  sample_heap();
  std::ranges::sort(cpu_);
  const auto heap = provider::HeapMeter::sample();
  Row taken{.elapsed = elapsed,
            .cycles = cycles_,
            .frames = frames_,
            .presented = presented_,
            .live = provider::AllocationMeter::live(),
            .allocations = provider::AllocationMeter::count(),
            .heap_bytes =
                heap.has_value() ? std::optional{heap->bytes} : std::nullopt,
            .heap_high_water = heap_high_water_,
            .cpu_p50 = percentile(cpu_, 50),
            .cpu_p95 = percentile(cpu_, 95),
            .cpu_p99 = percentile(cpu_, 99),
            .cpu_max = cpu_.empty() ? std::chrono::nanoseconds{0} : cpu_.back(),
            .suspensions =
                lifecycle_.has_value() ? lifecycle_->get().suspensions() : 0,
            .resumptions =
                lifecycle_.has_value() ? lifecycle_->get().resumptions() : 0};
  cpu_.clear();
  return taken;
}

auto Soak::growing(std::span<const std::int64_t> settled) -> bool {
  if (settled.size() < growth_window + 2) {
    return false;
  }
  const std::span<const std::int64_t> steady = settled.subspan(1);
  const bool rising =
      std::ranges::all_of(std::views::iota(std::size_t{1}, steady.size()),
                          [steady](std::size_t index) {
                            return steady[index] > steady[index - 1];
                          });
  return rising && steady.back() - steady.front() >= growth_floor;
}

auto Soak::csv_header() -> std::string_view {
  return "elapsed_s,cycles,frames,presented,live_allocations,allocations,"
         "heap_bytes,heap_high_water,cpu_p50_us,cpu_p95_us,cpu_p99_us,"
         "cpu_max_us,suspensions,resumptions";
}

auto Soak::csv(const Row& row) -> std::string {
  const auto micro = [](std::chrono::nanoseconds value) {
    return std::chrono::duration<double, std::micro>(value).count();
  };
  const auto optional = [](const auto& value) {
    return value.has_value() ? std::format("{}", *value) : std::string{};
  };
  return std::format(
      "{},{},{},{},{},{},{},{},{:.1f},{:.1f},{:.1f},{:.1f},{},{}",
      row.elapsed.count(), row.cycles, row.frames, row.presented,
      optional(row.live), optional(row.allocations), optional(row.heap_bytes),
      optional(row.heap_high_water), micro(row.cpu_p50), micro(row.cpu_p95),
      micro(row.cpu_p99), micro(row.cpu_max), row.suspensions, row.resumptions);
}

auto Soak::describe(const Row& row) -> std::string {
  const auto micro = [](std::chrono::nanoseconds value) {
    return std::chrono::duration<double, std::micro>(value).count();
  };
  const auto optional = [](const auto& value) {
    return value.has_value() ? std::format("{}", *value) : std::string{"-"};
  };
  return std::format(
      "soak {} s: {} cycles, {} frames ({} presented), live {}, heap {} (high "
      "{}), cpu p50 {:.0f} p95 {:.0f} p99 {:.0f} max {:.0f} us, suspend {} "
      "resume {}",
      row.elapsed.count(), row.cycles, row.frames, row.presented,
      optional(row.live), optional(row.heap_bytes),
      optional(row.heap_high_water), micro(row.cpu_p50), micro(row.cpu_p95),
      micro(row.cpu_p99), micro(row.cpu_max), row.suspensions, row.resumptions);
}

}
