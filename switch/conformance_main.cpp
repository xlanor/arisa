#include <arisa/conformance/conformance_suite.hpp>
#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/support/unique_handle.hpp>
#include <nxlink_log.hpp>

#include <switch.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <expected>
#include <format>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::nx {

struct ConsoleRelease {
  using handle_type = PrintConsole*;

  static auto empty() noexcept -> handle_type { return nullptr; }

  static auto is_valid(handle_type console) noexcept -> bool {
    return console != nullptr;
  }

  static void release(handle_type& console) noexcept { consoleExit(console); }
};

class ConformanceRun {
 public:
  static auto lines(const NxlinkLog& log,
                    const conformance::ConformanceOptions& options)
      -> std::vector<std::string> {
    std::vector<std::string> output;
    log.emit("factory create");
    auto factory = provider::horizon::HorizonProviderFactory::create();
    log.emit(factory.has_value() ? "factory created" : "factory failed");
    if (!factory.has_value()) {
      output.push_back(
          std::format("HorizonProviderFactory failed: {} (result {:#x})",
                      factory.error().description(),
                      factory.error().native_result().value_or(0)));
      return output;
    }
    const conformance::Report report =
        conformance::ConformanceSuite::run(*factory, options);
    output.push_back(std::format("arisa conformance ({}): {} checks, {} failed",
                                 factory->info().name, report.size(),
                                 report.failure_count()));
    for (const conformance::CheckResult& result : report) {
      output.push_back(std::format("{} {} {}", result.passed ? "PASS" : "FAIL",
                                   result.name, result.detail));
    }
    if (auto gpu = factory->stock_gpu_report()) {
      output.push_back(std::move(*gpu));
    }
    output.emplace_back(report.passed() ? "ALL PASSED" : "FAILURES PRESENT");
    for (const std::string& line : output) {
      log.emit(line);
    }
    log.emit("factory teardown begin");
    factory = std::unexpected(
        provider::ProviderError{provider::ProviderErrorCode::invalid_argument});
    log.emit("factory teardown done");
    return output;
  }
};

}

auto main(int argc, char* argv[]) -> int {
  const std::span arguments{argv, static_cast<std::size_t>(argc)};
  const arisa::conformance::ConformanceOptions options{
      .open_external = std::ranges::contains(
          arguments, std::string_view{"--open-external"},
          [](const char* argument) { return std::string_view{argument}; })};

  const arisa::nx::NxlinkLog trace{"conformance"};
  trace.emit("startup conformance");
  arisa::conformance::ConformanceOptions traced = options;
  traced.trace = [&trace](std::string_view message) { trace.emit(message); };
  const std::vector<std::string> lines =
      arisa::nx::ConformanceRun::lines(trace, traced);
  trace.emit("console init");

  const arisa::support::UniqueHandle<arisa::nx::ConsoleRelease> console{
      consoleInit(nullptr)};
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  PadState pad{};
  padInitializeDefault(&pad);
  for (const std::string& line : lines) {
    std::println("{}", line);
  }
  std::println("press + to exit");
  trace.emit("console ready");

  while (appletMainLoop()) {
    padUpdate(&pad);
    if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0) {
      break;
    }
    consoleUpdate(console.get());
  }
  trace.emit("exit conformance");
  return 0;
}
