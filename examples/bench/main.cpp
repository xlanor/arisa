#include <arisa/provider/headless/headless_factory.hpp>

#include <benchmark.hpp>
#include <host_assets.hpp>
#include <real_time_providers.hpp>

#include <chrono>
#include <cstddef>
#include <fstream>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace arisa::examples {

class HostBench {
 public:
  static constexpr std::chrono::milliseconds frame_step{16};
#if defined(__SANITIZE_ADDRESS__)
  static constexpr int slowdown = 4;
#else
  static constexpr int slowdown = 1;
#endif
  static constexpr Benchmark::Budget budget{
      .idle_cpu = std::chrono::microseconds{250},
      .scroll_cpu = std::chrono::microseconds{6000},
      .scroll_gpu = std::nullopt,
      .launch_cpu = std::chrono::microseconds{1500 * slowdown},
      .launch_uploaded = 2'000'000,
      .fling_repaint_vertex_bytes = 2048,
      .animation_wakes_per_second = 15.0,
      .blurs_per_frame = 0.5,
      .sweep_cpu = std::chrono::microseconds{4000 * slowdown},
      .sweep_frame = std::chrono::microseconds{16600 * slowdown},
      .still_cpu = std::chrono::microseconds{3000 * slowdown},
      .drift_cpu = std::chrono::microseconds{1000 * slowdown},
      .steps_cpu = std::chrono::microseconds{150 * slowdown},
      .sidebar_share = 0.2,
      .sheet_cpu = std::chrono::microseconds{100 * slowdown},
      .split_builds = 1,
      .fling_building_allocations = 180,
      .split_step_evaluations = 10,
      .paging_cpu = std::chrono::microseconds{250 * slowdown},
      .paging_layout = std::chrono::microseconds{2500 * slowdown},
      .paging_computations = 610.0,
      .paging_building_allocations = 820,
      .materials_reach = 56.0F,
      .paging_input = std::chrono::microseconds{400 * slowdown},
      .entry_cpu = std::chrono::microseconds{12000 * slowdown},
      .entry_computations = 4000,
      .idle_tick = std::chrono::microseconds{5 * slowdown},
      .token_drag_cpu = std::chrono::microseconds{3000 * slowdown},
      .interactions = {Benchmark::Interaction{
                           .scenario = Benchmark::Scenario::toggle_flip,
                           .cpu = std::chrono::microseconds{150 * slowdown},
                           .evaluations = 4.0,
                           .blurs = 0.0,
                           .ring = 2,
                           .outset = 16.0F},
                       Benchmark::Interaction{
                           .scenario = Benchmark::Scenario::slider_drag,
                           .cpu = std::chrono::microseconds{150 * slowdown},
                           .evaluations = 3.0,
                           .blurs = 0.0,
                           .ring = 2,
                           .outset = 16.0F},
                       Benchmark::Interaction{
                           .scenario = Benchmark::Scenario::list_walk,
                           .cpu = std::chrono::microseconds{300 * slowdown},
                           .evaluations = 8.0,
                           .blurs = 1.0,
                           .ring = 2,
                           .outset = 16.0F},
                       Benchmark::Interaction{
                           .scenario = Benchmark::Scenario::popover_menu,
                           .cpu = std::chrono::microseconds{200 * slowdown},
                           .evaluations = 10.0,
                           .blurs = 0.1,
                           .ring = 2,
                           .outset = 0.0F},
                       Benchmark::Interaction{
                           .scenario = Benchmark::Scenario::spinner_heavy,
                           .cpu = std::chrono::microseconds{150 * slowdown},
                           .evaluations = 0.0,
                           .blurs = 0.0,
                           .ring = 0,
                           .outset = 16.0F},
                       Benchmark::Interaction{
                           .scenario = Benchmark::Scenario::text_typing,
                           .cpu = std::chrono::microseconds{200 * slowdown},
                           .evaluations = 3.0,
                           .blurs = 0.0,
                           .ring = 2,
                           .outset = 16.0F},
                       Benchmark::Interaction{
                           .scenario = Benchmark::Scenario::clock_tick,
                           .cpu = std::chrono::microseconds{250 * slowdown},
                           .evaluations = 4.0,
                           .blurs = 0.5,
                           .ring = 0,
                           .outset = 16.0F}}};
  static constexpr std::chrono::nanoseconds vsync{16'666'667};

  static auto run(std::span<char*> arguments) -> int {
    std::vector<Benchmark::Scenario> chosen;
    std::string csv;
    bool budgets = false;
    bool real_time = false;
    bool render_thread = true;
    for (std::size_t index = 1; index < arguments.size(); ++index) {
      const std::string_view argument{arguments[index]};
      if (argument == "--budgets") {
        budgets = true;
      } else if (argument == "--real-time") {
        real_time = true;
      } else if (argument == "--inline") {
        render_thread = false;
      } else if (argument == "--csv" && index + 1 < arguments.size()) {
        csv = arguments[++index];
      } else if (argument == "--scenario" && index + 1 < arguments.size()) {
        const auto parsed = Benchmark::parse(arguments[++index]);
        if (!parsed.has_value()) {
          std::println(stderr, "unknown scenario {}", arguments[index]);
          return 2;
        }
        chosen.push_back(*parsed);
      } else {
        std::println(stderr,
                     "usage: arisa_bench [--scenario name]... [--csv path] "
                     "[--budgets] [--real-time] [--inline]");
        return 2;
      }
    }
    if (chosen.empty()) {
      chosen.assign(Benchmark::scenarios.begin(), Benchmark::scenarios.end());
    }
    provider::headless::HeadlessProviderFactory providers;
    HostAssets::load(providers);
    RealTimeProviders paced{providers};
    std::chrono::steady_clock::time_point next =
        std::chrono::steady_clock::now();
    Benchmark bench =
        real_time ? Benchmark{paced, providers.input(),
                              [&next](std::chrono::nanoseconds skip) {
                                next += vsync + skip;
                                std::this_thread::sleep_until(next);
                                return true;
                              },
                              true, render_thread}
                  : Benchmark{providers, providers.input(),
                              [&providers](std::chrono::nanoseconds skip) {
                                return providers.clock()
                                    .advance(frame_step + skip)
                                    .has_value();
                              },
                              false, render_thread};
    int failures = 0;
    for (const Benchmark::Scenario scenario : chosen) {
      next = std::chrono::steady_clock::now();
      auto summary = bench.run(scenario);
      if (!summary.has_value()) {
        std::println(stderr, "bench {}: {}", Benchmark::name(scenario),
                     summary.error().explanation());
        return 1;
      }
      std::println("{}", Benchmark::describe(*summary));
      if (budgets) {
        for (const std::string& violation :
             Benchmark::violations(*summary, budget)) {
          std::println(stderr, "budget: {}", violation);
          ++failures;
        }
      }
    }
    if (!csv.empty()) {
      std::ofstream out{csv};
      out << Benchmark::csv_header() << '\n';
      for (const Benchmark::Sample& sample : bench.samples()) {
        out << Benchmark::csv(sample) << '\n';
      }
    }
    return failures == 0 ? 0 : 1;
  }
};

}

auto main(int argc, char** argv) -> int {
  return arisa::examples::HostBench::run(
      std::span{argv, static_cast<std::size_t>(argc)});
}
