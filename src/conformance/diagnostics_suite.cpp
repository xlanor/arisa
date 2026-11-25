#include <arisa/conformance/diagnostics_suite.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace arisa::conformance {

void DiagnosticsSuite::run(provider::ProviderFactory& factory, Report& report) {
  check_concurrent_reports(factory, report);
}

void DiagnosticsSuite::check_concurrent_reports(
    provider::ProviderFactory& factory, Report& report) {
  static constexpr std::size_t reporters = 4;
  static constexpr int reports_each = 250;
  provider::DiagnosticsProvider& diagnostics = factory.diagnostics();
  std::atomic<int> delivered{0};
  bool all_started = true;
  {
    std::array<std::optional<provider::Thread>, reporters> threads;
    for (std::size_t index = 0; index < threads.size(); ++index) {
      auto thread = factory.threading().start(
          provider::ThreadSpec{.name = std::format("arisa-diag-{}", index),
                               .role = provider::ThreadRole::background},
          [&diagnostics, &delivered, index](const std::stop_token&) {
            for (int sequence = 0; sequence < reports_each; ++sequence) {
              diagnostics.report(provider::Diagnostic{
                  .severity = provider::Severity::debug,
                  .category = "arisa.conformance",
                  .message =
                      std::format("reporter {} report {}", index, sequence)});
              delivered.fetch_add(1);
            }
          });
      if (thread.has_value()) {
        threads[index].emplace(std::move(*thread));
      } else {
        all_started = false;
      }
    }
  }
  report.record("diagnostics.concurrent_reports",
                all_started && delivered.load() ==
                                   static_cast<int>(reporters) * reports_each,
                "concurrent reports from four threads did not all complete");
}

}
