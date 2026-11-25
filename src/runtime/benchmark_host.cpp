#include <arisa/runtime/benchmark_host.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace arisa::runtime {

auto BenchmarkHost::run(std::size_t frames, bool until_idle)
    -> std::expected<Measurement, ApplicationError> {
  Measurement measured;
  const std::size_t limit = until_idle ? settle_limit : frames;
  for (std::size_t index = 0; index < limit; ++index) {
    if (auto framed = application_.get().frame(); !framed.has_value()) {
      return std::unexpected(framed.error());
    }
    const FrameTimings& last = application_.get().last_frame();
    ++measured.frames;
    if (last.kind != FrameKind::idle) {
      ++measured.rendered;
    }
    measured.total += last.wall;
    measured.worst = std::max(measured.worst, last.wall);
    if (until_idle && index + 1 >= frames && last.kind == FrameKind::idle) {
      break;
    }
  }
  return measured;
}

auto BenchmarkHost::measure_render(std::chrono::nanoseconds interval)
    -> std::expected<Measurement, ApplicationError> {
  static_cast<void>(interval);
  return run(1, false);
}

auto BenchmarkHost::measure_renders(std::chrono::nanoseconds duration)
    -> std::expected<Measurement, ApplicationError> {
  const auto count = static_cast<std::size_t>(std::max<std::int64_t>(
      1, (duration + nominal_frame - std::chrono::nanoseconds{1}) /
             nominal_frame));
  return run(count, false);
}

auto BenchmarkHost::measure_renders(double seconds)
    -> std::expected<Measurement, ApplicationError> {
  return measure_renders(
      std::chrono::nanoseconds{std::llround(seconds * 1.0e9)});
}

auto BenchmarkHost::measure_action(std::move_only_function<void()> action)
    -> std::expected<Measurement, ApplicationError> {
  if (action) {
    action();
  }
  return run(1, true);
}

auto BenchmarkHost::view_for_identifier(const composition::ElementKey& key)
    const -> std::optional<Introspection::NodeRecord> {
  const auto id = application_.get().root().tree().find(key);
  if (!id.has_value()) {
    return std::nullopt;
  }
  return application_.get().introspection().node(*id);
}

}
