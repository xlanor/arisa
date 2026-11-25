#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class InputSuite {
 public:
  static constexpr float probe_deadzone = 0.3F;

  static void run(provider::ProviderFactory& factory, Report& report);

  static auto consistent(const provider::InputFrame& frame) noexcept -> bool;

 private:
  static void check_sample(provider::InputProvider& input, Report& report);
  static void check_tuning(provider::InputProvider& input, Report& report);
  static void check_absent(provider::InputProvider& input, Report& report);

  static auto within_unit(const provider::Stick& stick) noexcept -> bool;
};

}
