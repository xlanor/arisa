#include <arisa/conformance/haptics_suite.hpp>

#include <algorithm>

namespace arisa::conformance {

using provider::ProviderErrorCode;

void HapticsSuite::run(provider::ProviderFactory& factory, Report& report) {
  check_patterns(factory.haptics(),
                 factory.supplies(provider::Capability::haptics), report);
}

void HapticsSuite::check_patterns(provider::HapticsProvider& haptics,
                                  bool supplied, Report& report) {
  const bool conforms = std::ranges::all_of(
      provider::SensoryFeedback::catalogue(),
      [&haptics, supplied](const provider::SensoryFeedback& feedback) {
        const auto played = haptics.play(feedback, provider::HapticStrength{});
        const bool unsupported =
            !played.has_value() &&
            played.error().code() == ProviderErrorCode::unsupported_capability;
        return supplied != unsupported;
      });
  report.record(supplied ? "haptics.play" : "haptics.absent", conforms,
                supplied ? "play() reported unsupported_capability while "
                           "supplied"
                         : "an unsupplied haptics provider did not return "
                           "unsupported_capability");
}

}
