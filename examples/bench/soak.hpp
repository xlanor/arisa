#pragma once

#include <arisa/provider/factory.hpp>
#include <arisa/provider/headless/scripted_input.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/application_error.hpp>

#include <gallery_page.hpp>
#include <soak_lifecycle.hpp>
#include <soak_screen.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class Soak {
 public:
  struct Options {
    std::chrono::seconds duration{std::chrono::minutes{120}};
    std::chrono::seconds interval{std::chrono::minutes{1}};
  };

  struct Row {
    std::chrono::seconds elapsed{0};
    std::uint32_t cycles{0};
    std::uint64_t frames{0};
    std::uint64_t presented{0};
    std::optional<std::int64_t> live{};
    std::optional<std::uint64_t> allocations{};
    std::optional<std::uint64_t> heap_bytes{};
    std::optional<std::uint64_t> heap_high_water{};
    std::chrono::nanoseconds cpu_p50{0};
    std::chrono::nanoseconds cpu_p95{0};
    std::chrono::nanoseconds cpu_p99{0};
    std::chrono::nanoseconds cpu_max{0};
    std::uint32_t suspensions{0};
    std::uint32_t resumptions{0};
  };

  struct Report {
    std::vector<Row> rows;
    std::vector<std::int64_t> settled_live;
  };

  using Advance = std::move_only_function<bool()>;
  using Sink = std::move_only_function<void(const Row&)>;
  using Lifecycle = std::optional<std::reference_wrapper<const SoakLifecycle>>;

  static constexpr std::uint32_t page_frames = 45;
  static constexpr std::uint32_t theme_every = 6;
  static constexpr std::uint32_t drag_start = 10;
  static constexpr std::uint32_t drag_steps = 6;
  static constexpr std::uint32_t mount_frames = 30;
  static constexpr std::uint32_t toggle_frames = 30;
  static constexpr std::uint32_t toggles = 12;
  static constexpr std::uint32_t strokes = 4;
  static constexpr std::uint32_t stroke_frames = 60;
  static constexpr std::uint32_t settle_frames = 120;
  static constexpr std::uint32_t heap_period = 60;
  static constexpr std::size_t growth_window = 4;
  static constexpr std::int64_t growth_floor = 64;
  static constexpr float drag_distance = 0.4F;

  Soak(provider::ProviderFactory& providers,
       provider::headless::ScriptedInput& input, Advance advance,
       bool real_time, Lifecycle lifecycle, Sink sink) noexcept;

  [[nodiscard]] auto run(Options options)
      -> std::expected<Report, runtime::ApplicationError>;

  static auto csv_header() -> std::string_view;

  static auto csv(const Row& row) -> std::string;

  static auto describe(const Row& row) -> std::string;

  static auto growing(std::span<const std::int64_t> settled) -> bool;

 private:
  struct Phase {
    SoakScreen::Mode mode;
    std::uint32_t frames;
  };

  static constexpr std::array phases{
      Phase{.mode = SoakScreen::Mode::gallery,
            .frames = static_cast<std::uint32_t>(GalleryPages::all.size()) *
                      page_frames},
      Phase{.mode = SoakScreen::Mode::fling,
            .frames = mount_frames + (strokes * stroke_frames)},
      Phase{.mode = SoakScreen::Mode::sheet,
            .frames = mount_frames + (toggles * toggle_frames)},
      Phase{.mode = SoakScreen::Mode::navigation,
            .frames = mount_frames + (toggles * toggle_frames)},
      Phase{.mode = SoakScreen::Mode::gallery, .frames = settle_frames}};

  void stimulate(std::uint32_t phase, std::uint32_t frame);

  void gallery(std::uint32_t frame);

  void press(provider::Button button);

  void drag(std::uint32_t step, bool down);

  void sample_heap();

  auto row(std::chrono::seconds elapsed) -> Row;

  static auto percentile(std::span<const std::chrono::nanoseconds> sorted,
                         std::uint32_t share) -> std::chrono::nanoseconds;

  std::reference_wrapper<provider::ProviderFactory> providers_;
  std::reference_wrapper<provider::headless::ScriptedInput> input_;
  Advance advance_;
  bool real_time_;
  Lifecycle lifecycle_;
  Sink sink_;
  std::shared_ptr<SoakScreen::Remote> remote_;
  std::vector<std::chrono::nanoseconds> cpu_;
  std::uint32_t cycles_{0};
  std::uint64_t frames_{0};
  std::uint64_t presented_{0};
  std::optional<std::uint64_t> heap_high_water_{};
};

}
