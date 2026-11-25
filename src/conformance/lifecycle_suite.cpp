#include <arisa/conformance/lifecycle_suite.hpp>

#include <optional>

namespace arisa::conformance {

using provider::Capability;
using provider::ProviderErrorCode;
using provider::ScenePhase;

void LifecycleSuite::run(provider::ProviderFactory& factory, Report& report) {
  provider::LifecycleProvider& lifecycle = factory.lifecycle();
  if (!factory.supplies(Capability::lifecycle)) {
    check_absent(lifecycle, report);
    return;
  }
  check_phase(lifecycle, report);
  check_poll_drains(lifecycle, report);
  check_home_button_block(lifecycle, report);
}

void LifecycleSuite::check_phase(const provider::LifecycleProvider& lifecycle,
                                 Report& report) {
  const ScenePhase phase = lifecycle.phase();
  report.record("lifecycle.phase_valid",
                phase == ScenePhase::active || phase == ScenePhase::inactive ||
                    phase == ScenePhase::background,
                "phase() returned a value outside ScenePhase");
}

void LifecycleSuite::check_poll_drains(provider::LifecycleProvider& lifecycle,
                                       Report& report) {
  bool drained = false;
  for (int call = 0; call < poll_limit && !drained; ++call) {
    drained = !lifecycle.poll().has_value();
  }
  report.record("lifecycle.poll_drains", drained,
                "poll() kept returning events for 1000 calls");
}

void LifecycleSuite::check_home_button_block(
    provider::LifecycleProvider& lifecycle, Report& report) {
  bool passed = false;
  {
    const auto outer = lifecycle.block_home_button();
    if (outer.has_value()) {
      const auto inner = lifecycle.block_home_button();
      passed = outer->active() && inner.has_value() && inner->active();
    } else {
      passed =
          outer.error().code() == ProviderErrorCode::unsupported_capability;
    }
  }
  report.record("lifecycle.home_button_block", passed,
                "block_home_button() failed or returned an inactive scope");
}

void LifecycleSuite::check_absent(provider::LifecycleProvider& lifecycle,
                                  Report& report) {
  const auto block = lifecycle.block_home_button();
  report.record(
      "lifecycle.absent",
      lifecycle.phase() == ScenePhase::active &&
          !lifecycle.poll().has_value() && !block.has_value() &&
          block.error().code() == ProviderErrorCode::unsupported_capability,
      "an unsupplied lifecycle provider did not behave as absent");
}

}
