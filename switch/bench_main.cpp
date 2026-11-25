#include <arisa/provider/headless/scripted_input.hpp>
#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/provider/providers.hpp>

#include <benchmark.hpp>
#include <nxlink_log.hpp>
#include <switch_console.hpp>
#include <ui_thread.hpp>

#include <switch.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <string>
#include <system_error>

namespace arisa::nx {

class SwitchBench {
 public:
  static constexpr std::string_view directory = "sdmc:/arisa/bench";

  static auto run() -> int {
    const NxlinkLog log{"bench"};
    log.emit("startup bench");
    const std::string outcome = measure(log);
    log.emit("exit bench: " + outcome);
    SwitchConsole::show(outcome);
    return 0;
  }

 private:
  static auto measure(const NxlinkLog& log) -> std::string {
    provider::headless::ScriptedInput input;
    auto factory = provider::horizon::HorizonProviderFactory::create(
        provider::Providers{.input = std::ref(input)});
    if (!factory.has_value()) {
      return std::string{"HorizonProviderFactory failed: "} +
             factory.error().explanation();
    }
    examples::Benchmark bench{
        *factory, input,
        [](std::chrono::nanoseconds) { return appletMainLoop(); }, true};
    std::string report;
    for (const examples::Benchmark::Scenario scenario :
         examples::Benchmark::scenarios) {
      auto summary = bench.run(scenario);
      if (!summary.has_value()) {
        return std::format("bench {} failed: {}",
                           examples::Benchmark::name(scenario),
                           summary.error().explanation());
      }
      const std::string line = examples::Benchmark::describe(*summary);
      log.emit(line);
      report += line + '\n';
    }
    const std::string path =
        std::format("{}/{:%Y%m%d-%H%M%S}.csv", directory,
                    std::chrono::floor<std::chrono::seconds>(
                        std::chrono::system_clock::now()));
    std::error_code failed;
    std::filesystem::create_directories(directory, failed);
    std::ofstream out{path};
    if (!out) {
      return report + "could not write " + path;
    }
    out << examples::Benchmark::csv_header() << '\n';
    for (const examples::Benchmark::Sample& sample : bench.samples()) {
      out << examples::Benchmark::csv(sample) << '\n';
    }
    log.emit("bench csv " + path);
    return report + "wrote " + path;
  }
};

}

auto main() -> int {
  return arisa::nx::UiThread::run([] { return arisa::nx::SwitchBench::run(); });
}
