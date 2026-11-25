#pragma once

#include <arisa/composition/node.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/provider/diagnostics/thread_cpu_meter.hpp>
#include <arisa/provider/factory.hpp>
#include <arisa/provider/headless/scripted_input.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_error.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/frame_timings.hpp>

#include <bench_screen.hpp>
#include <interaction_screen.hpp>
#include <launch_screen.hpp>
#include <split_screen.hpp>

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

class Benchmark {
 public:
  enum class Scenario : std::uint8_t {
    idle,
    fling,
    gallery,
    navigation,
    sheet,
    theme,
    launch,
    launch_path,
    sidebar_scroll,
    detail_scroll,
    sidebar_fast,
    glass_page,
    materials_page,
    covered_page,
    sidebar_steps,
    sidebar_settle,
    sheet_glass,
    split_steps,
    toggle_flip,
    slider_drag,
    list_walk,
    popover_menu,
    spinner_heavy,
    text_typing,
    clock_tick,
    list_paging,
    materials_leave,
    tour,
    launch_materials,
    lists_idle,
    lists_enter,
    lists_return,
    token_slider_drag,
  };

  struct Sample {
    Scenario scenario{Scenario::idle};
    std::uint32_t frame{0};
    runtime::FrameKind kind{runtime::FrameKind::idle};
    bool presented{false};
    bool laid_out{false};
    bool stimulus{false};
    bool released{false};
    std::uint64_t evaluations{0};
    std::chrono::nanoseconds cpu{0};
    std::chrono::nanoseconds wall{0};
    std::optional<std::chrono::nanoseconds> gpu{};
    std::optional<std::uint64_t> allocations{};
    std::uint64_t uploaded_bytes{0};
    std::uint64_t uploaded_vertices{0};
    std::uint64_t damaged_pixels{0};
    std::uint32_t blurs{0};
    std::uint32_t recorded{0};
    std::uint32_t spliced{0};
    std::size_t nodes{0};
    std::chrono::nanoseconds reconcile{0};
    std::chrono::nanoseconds layout{0};
    std::chrono::nanoseconds paint{0};
    std::uint32_t row{0};
    bool animating{false};
    bool paced{false};
    std::uint64_t computations{0};
    std::chrono::nanoseconds input{0};
    std::uint64_t collections{0};
    std::uint64_t refreshes{0};
    std::uint64_t uncovered{0};
    runtime::FrameTimings timings{};
  };

  struct Summary {
    Scenario scenario{Scenario::idle};
    std::uint32_t frames{0};
    std::uint32_t presented{0};
    std::uint32_t repaints{0};
    std::chrono::nanoseconds wall_mean{0};
    std::chrono::nanoseconds cpu_mean{0};
    std::chrono::nanoseconds cpu_p95{0};
    std::chrono::nanoseconds cpu_max{0};
    std::optional<std::chrono::nanoseconds> gpu_mean{};
    std::optional<std::chrono::nanoseconds> gpu_max{};
    std::optional<std::uint64_t> allocations{};
    std::optional<std::uint64_t> repaint_allocations{};
    std::uint64_t uploaded_bytes_mean{0};
    std::uint64_t vertex_bytes_mean{0};
    std::uint64_t repaint_vertex_bytes_mean{0};
    std::uint64_t damaged_pixels_mean{0};
    std::uint64_t damaged_pixels_median{0};
    std::uint64_t surface_pixels{0};
    std::chrono::nanoseconds elapsed{0};
    std::vector<provider::ThreadCpu> threads{};
    std::uint64_t animation_wakes{0};
    std::uint64_t blurs{0};
    std::uint64_t blur_hits{0};
    std::uint32_t still_frames{0};
    std::uint32_t still_presented{0};
    std::uint64_t still_blurs{0};
    std::chrono::nanoseconds still_cpu_mean{0};
    std::chrono::nanoseconds drift_cpu_mean{0};
    std::uint32_t steady_recorded_max{0};
    std::size_t nodes_min{0};
    std::size_t nodes_max{0};
    std::uint64_t detail_builds{0};
    std::uint32_t building{0};
    std::uint64_t building_allocations{0};
    std::uint32_t steps{0};
    std::uint64_t evaluations{0};
    std::uint64_t damaged_pixels_max{0};
    std::uint32_t recorded_max{0};
    std::size_t control_nodes{0};
    std::size_t control_depth{0};
    std::uint64_t region_pixels{0};
    composition::Rect region{};
    float scale{1.0F};
    std::uint64_t computations{0};
    std::uint32_t laid_out{0};
    std::chrono::nanoseconds layout_mean{0};
    std::uint32_t quiet_presented{0};
    std::uint32_t resumed_presented{0};
    std::uint64_t still_damaged_max{0};
    std::uint32_t deepest{0};
    std::chrono::nanoseconds input_mean{0};
    std::uint64_t collections{0};
    std::uint64_t quiet_collections{0};
    std::uint64_t searches{0};
    std::uint64_t entry_computations{0};
    std::uint32_t worst_frame{0};
    std::chrono::nanoseconds worst_wall{0};
    std::optional<std::uint64_t> worst_allocations{};
    runtime::FrameTimings worst{};
  };

  struct Interaction {
    Scenario scenario{Scenario::idle};
    std::chrono::nanoseconds cpu{0};
    double evaluations{0.0};
    double blurs{0.0};
    std::uint32_t ring{0};
    float outset{0.0F};
  };

  struct Budget {
    std::chrono::nanoseconds idle_cpu{0};
    std::chrono::nanoseconds scroll_cpu{0};
    std::optional<std::chrono::nanoseconds> scroll_gpu{};
    std::chrono::nanoseconds launch_cpu{0};
    std::uint64_t launch_uploaded{0};
    std::uint64_t fling_repaint_vertex_bytes{0};
    double animation_wakes_per_second{0.0};
    double blurs_per_frame{0.0};
    std::chrono::nanoseconds sweep_cpu{0};
    std::chrono::nanoseconds sweep_frame{0};
    std::chrono::nanoseconds still_cpu{0};
    std::chrono::nanoseconds drift_cpu{0};
    std::chrono::nanoseconds steps_cpu{0};
    double sidebar_share{0.0};
    std::chrono::nanoseconds sheet_cpu{0};
    std::uint64_t split_builds{0};
    std::uint64_t fling_building_allocations{0};
    std::uint64_t split_step_evaluations{0};
    std::chrono::nanoseconds paging_cpu{0};
    std::chrono::nanoseconds paging_layout{0};
    double paging_computations{0.0};
    std::uint64_t paging_building_allocations{0};
    float materials_reach{0.0F};
    std::chrono::nanoseconds paging_input{0};
    std::chrono::nanoseconds entry_cpu{0};
    std::uint64_t entry_computations{0};
    std::chrono::nanoseconds idle_tick{0};
    std::chrono::nanoseconds token_drag_cpu{0};
    std::array<Interaction, 7> interactions{};
  };

  using Advance = std::move_only_function<bool(std::chrono::nanoseconds)>;

  static constexpr std::array scenarios{Scenario::idle,
                                        Scenario::fling,
                                        Scenario::gallery,
                                        Scenario::navigation,
                                        Scenario::sheet,
                                        Scenario::theme,
                                        Scenario::launch,
                                        Scenario::launch_path,
                                        Scenario::sidebar_scroll,
                                        Scenario::detail_scroll,
                                        Scenario::sidebar_fast,
                                        Scenario::glass_page,
                                        Scenario::materials_page,
                                        Scenario::covered_page,
                                        Scenario::sidebar_steps,
                                        Scenario::sidebar_settle,
                                        Scenario::sheet_glass,
                                        Scenario::split_steps,
                                        Scenario::toggle_flip,
                                        Scenario::slider_drag,
                                        Scenario::list_walk,
                                        Scenario::popover_menu,
                                        Scenario::spinner_heavy,
                                        Scenario::text_typing,
                                        Scenario::clock_tick,
                                        Scenario::list_paging,
                                        Scenario::materials_leave,
                                        Scenario::tour,
                                        Scenario::launch_materials,
                                        Scenario::lists_idle,
                                        Scenario::lists_enter,
                                        Scenario::lists_return,
                                        Scenario::token_slider_drag};

  static constexpr std::uint32_t fling_strokes = 12;
  static constexpr std::uint32_t fling_period = 60;
  static constexpr std::uint32_t fling_drag = 6;
  static constexpr std::uint32_t page_frames = 45;
  static constexpr std::uint32_t cycles = 20;
  static constexpr std::uint32_t cycle_frames = 60;
  static constexpr std::uint32_t theme_switches = 10;
  static constexpr std::uint32_t theme_frames = 30;
  static constexpr std::uint32_t idle_frames = 600;
  static constexpr std::uint32_t launch_frames = 300;
  static constexpr std::uint32_t warmup = 60;
  static constexpr std::uint32_t idle_warmup = 240;
  static constexpr float fling_distance = 0.42F;
  static constexpr std::uint32_t app_task_share = 100;
  static constexpr std::uint32_t scroll_strokes = 8;
  static constexpr std::uint32_t scroll_period = 40;
  static constexpr float sidebar_column = 0.09F;
  static constexpr float detail_column = 0.6F;
  static constexpr float glass_scale = 1.25F;
  static constexpr std::uint32_t sweep_period = 5;
  static constexpr std::uint32_t sweep_tail = 30;
  static constexpr std::uint32_t step_presses = 40;
  static constexpr std::uint32_t step_period = 6;
  static constexpr std::uint32_t settle_presses = 8;
  static constexpr std::uint32_t settle_period = 30;
  static constexpr std::uint32_t still_frames = 120;
  static constexpr std::uint32_t cover_frames = 90;
  static constexpr double nominal_rate = 60.0;
  static constexpr std::uint32_t flips = 16;
  static constexpr std::uint32_t flip_period = 30;
  static constexpr std::uint32_t slider_strokes = 4;
  static constexpr std::uint32_t slider_period = 30;
  static constexpr std::uint32_t slider_presses = 20;
  static constexpr std::uint32_t token_strokes = 4;
  static constexpr std::uint32_t token_period = 60;
  static constexpr std::uint32_t token_drag = 48;
  static constexpr std::string_view dragged_token = "colours.accent.light";
  static constexpr std::uint32_t menu_cycles = 6;
  static constexpr std::uint32_t menu_period = 120;
  static constexpr std::uint32_t keystrokes = 32;
  static constexpr std::uint32_t ticks = 4;
  static constexpr std::uint32_t tick_period = 120;
  static constexpr std::chrono::milliseconds tick_lead{500};
  static constexpr std::uint32_t paging_entry = 60;
  static constexpr std::uint32_t paging_hold = 120;
  static constexpr std::uint32_t paging_release = 30;
  static constexpr std::uint32_t paging_period = 40;
  static constexpr std::uint32_t paging_forward = 10;
  static constexpr std::uint32_t paging_back = 5;
  static constexpr std::uint32_t paging_tail = 180;
  static constexpr std::uint32_t leave_tail = 240;
  static constexpr std::uint32_t leave_quiet = 30;
  static constexpr std::uint32_t tour_stay = 120;
  static constexpr std::uint32_t tour_tail = 600;
  static constexpr std::uint32_t launch_materials_frames = 1800;
  static constexpr int long_rows = 1000;
  static constexpr int long_key = 100000;
  static constexpr std::uint32_t paged_rows = 60;
  static constexpr std::string_view typed_phrase =
      "The quick brown fox jumps over t";

  Benchmark(provider::ProviderFactory& providers,
            provider::headless::ScriptedInput& input, Advance advance,
            bool real_time, bool render_thread = true) noexcept;

  static auto name(Scenario scenario) noexcept -> std::string_view;

  static auto parse(std::string_view name) noexcept -> std::optional<Scenario>;

  [[nodiscard]] auto run(Scenario scenario)
      -> std::expected<Summary, runtime::ApplicationError>;

  auto samples() const noexcept -> std::span<const Sample> { return samples_; }

  static auto csv_header() -> std::string_view;

  static auto csv(const Sample& sample) -> std::string;

  static auto describe(const Summary& summary) -> std::string;

  static auto threads(const Summary& summary) -> std::string;

  static auto violations(const Summary& summary, const Budget& budget)
      -> std::vector<std::string>;

  static auto thread_share(const Summary& summary, std::string_view prefix)
      -> double;

 private:
  struct Plan {
    std::uint32_t warmup{0};
    std::uint32_t frames{0};
  };

  static auto plan(Scenario scenario) noexcept -> Plan;

  static auto rehearsed(Scenario scenario) noexcept -> bool;

  static auto worst_from(Scenario scenario) noexcept -> std::uint32_t;

  static auto paged(Scenario scenario) noexcept -> bool;

  static auto still(Scenario scenario, std::uint32_t frame) noexcept -> bool;

  static auto quiet(Scenario scenario, std::uint32_t frame) noexcept -> bool;

  static auto resumed(Scenario scenario, std::uint32_t frame) noexcept -> bool;

  static auto steady(const Sample& sample) noexcept -> bool;

  static auto interactive(Scenario scenario) noexcept -> bool;

  static auto mode(Scenario scenario) noexcept -> InteractionScreen::Mode;

  static auto subtree(const composition::Node& node) -> std::size_t;

  static auto depth(composition::Tree& tree, const composition::Node& node)
      -> std::size_t;

  auto region(runtime::Application& application, std::string_view key) const
      -> std::optional<composition::Rect>;

  auto area(runtime::Application& application, Scenario scenario) const
      -> composition::Rect;

  static auto pixels(const Summary& summary, float outset) -> std::uint64_t;

  static auto interaction_violations(const Summary& summary,
                                     const Budget& budget)
      -> std::vector<std::string>;

  auto content(Scenario scenario) -> composition::Element;

  auto photo() -> LaunchScreen::Photo;

  auto stimulate(Scenario scenario, std::uint32_t frame,
                 runtime::Application& application) -> bool;

  auto slide(std::uint32_t frame, runtime::Application& application) -> bool;

  auto edit_token(std::uint32_t frame, runtime::Application& application)
      -> bool;

  auto walk(std::uint32_t frame) -> bool;

  auto cycle(std::uint32_t frame) -> bool;

  auto type(std::uint32_t frame) -> bool;

  auto tick(std::uint32_t frame) -> bool;

  auto page(std::uint32_t frame) -> bool;

  void hold(provider::Button button);

  static auto deepest(runtime::Application& application) -> std::uint32_t;

  void press(provider::Button button);

  void touch(provider::PixelPoint point, provider::TouchPhase phase);

  auto stroke(std::uint32_t frame) -> bool;

  auto scroll(std::uint32_t frame, float column) -> bool;

  auto sweep(std::uint32_t frame) -> bool;
  auto step(std::uint32_t frame) -> bool;

  auto settle(std::uint32_t frame) -> bool;

  auto split(std::uint32_t frame) -> bool;

  void drag(std::uint32_t step, std::uint32_t drag_frames, bool down,
            float column);

  static auto glass_appearance() -> runtime::AppearanceOptions;

  auto summarise(Scenario scenario, std::size_t first) const -> Summary;

  std::reference_wrapper<provider::ProviderFactory> providers_;
  std::reference_wrapper<provider::headless::ScriptedInput> input_;
  Advance advance_;
  bool real_time_;
  bool render_thread_;
  std::shared_ptr<BenchScreen::Remote> remote_;
  std::shared_ptr<SplitScreen::Remote> split_;
  std::shared_ptr<InteractionScreen::Remote> interaction_;
  std::chrono::nanoseconds skip_{0};
  std::uint32_t pressed_{0};
  std::vector<Sample> samples_;
  std::optional<LaunchScreen::Photo> photo_;
};

}
