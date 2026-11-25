#include <arisa/conformance/input_suite.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <flat_set>

namespace arisa::conformance {

void InputSuite::run(provider::ProviderFactory& factory, Report& report) {
  provider::InputProvider& input = factory.input();
  if (!factory.supplies(provider::Capability::input)) {
    check_absent(input, report);
    return;
  }
  check_sample(input, report);
  check_tuning(input, report);
}

auto InputSuite::consistent(const provider::InputFrame& frame) noexcept
    -> bool {
  const bool pads =
      std::ranges::all_of(frame.pads, [](const provider::PadState& pad) {
        if (!pad.connected) {
          return pad.buttons.empty() && pad.left == provider::Stick{} &&
                 pad.right == provider::Stick{};
        }
        return within_unit(pad.left) && within_unit(pad.right);
      });
  std::flat_set<std::uint64_t> ids;
  const bool touches =
      std::ranges::all_of(frame.touches, [&ids](const provider::Touch& touch) {
        return ids.insert(touch.id.value()).second &&
               std::isfinite(touch.position.x) &&
               std::isfinite(touch.position.y) && touch.position.x >= 0.0F &&
               touch.position.y >= 0.0F;
      });
  return pads && touches;
}

auto InputSuite::within_unit(const provider::Stick& stick) noexcept -> bool {
  static constexpr float tolerance = 1.0e-4F;
  return std::isfinite(stick.x) && std::isfinite(stick.y) &&
         std::hypot(stick.x, stick.y) <= 1.0F + tolerance;
}

void InputSuite::check_sample(provider::InputProvider& input, Report& report) {
  const provider::InputFrame first = input.sample();
  const provider::InputFrame second = input.sample();
  report.record("input.sample_consistent",
                consistent(first) && consistent(second),
                "sample() returned a disconnected pad with state, a stick "
                "outside the unit circle, or a duplicate or invalid touch");
}

void InputSuite::check_tuning(provider::InputProvider& input, Report& report) {
  const provider::InputTuning original = input.tuning();
  const auto probe = provider::InputTuning::create(probe_deadzone);
  bool passed = probe.has_value();
  if (passed) {
    input.set_tuning(*probe);
    passed = input.tuning() == *probe;
    input.set_tuning(original);
    passed = passed && input.tuning() == original;
  }
  report.record("input.tuning", passed,
                "set_tuning() did not change what tuning() returns");
}

void InputSuite::check_absent(provider::InputProvider& input, Report& report) {
  report.record("input.absent", input.sample() == provider::InputFrame{},
                "an unsupplied input provider returned a non-empty frame");
}

}
