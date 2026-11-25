#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class ClockSuite {
 public:
  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_monotonic(const provider::ClockProvider& clock,
                              Report& report);
  static void check_wall_clock(const provider::ClockProvider& clock,
                               Report& report);
};

}
