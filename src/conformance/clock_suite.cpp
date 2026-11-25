#include <arisa/conformance/clock_suite.hpp>

#include <chrono>

namespace arisa::conformance {

void ClockSuite::run(provider::ProviderFactory& factory, Report& report) {
  const provider::ClockProvider& clock = factory.clock();
  check_monotonic(clock, report);
  check_wall_clock(clock, report);
}

void ClockSuite::check_monotonic(const provider::ClockProvider& clock,
                                 Report& report) {
  static constexpr int reads = 10'000;
  provider::Instant previous = clock.now();
  bool ordered = true;
  for (int read = 0; read < reads; ++read) {
    const provider::Instant current = clock.now();
    ordered = ordered && current >= previous;
    previous = current;
  }
  report.record("clock.monotonic", ordered,
                "now() went backwards across 10000 reads");
}

void ClockSuite::check_wall_clock(const provider::ClockProvider& clock,
                                  Report& report) {
  static constexpr std::chrono::sys_days earliest{std::chrono::year{2020} /
                                                  std::chrono::January / 1};
  report.record("clock.wall_after_2020", clock.wall_now() > earliest,
                "wall_now() is earlier than 2020-01-01");
}

}
