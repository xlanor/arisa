#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class LifecycleSuite {
 public:
  static constexpr int poll_limit = 1000;

  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_phase(const provider::LifecycleProvider& lifecycle,
                          Report& report);
  static void check_poll_drains(provider::LifecycleProvider& lifecycle,
                                Report& report);
  static void check_home_button_block(provider::LifecycleProvider& lifecycle,
                                      Report& report);
  static void check_absent(provider::LifecycleProvider& lifecycle,
                           Report& report);
};

}
