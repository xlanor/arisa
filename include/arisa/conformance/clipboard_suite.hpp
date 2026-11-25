#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

#include <string_view>

namespace arisa::conformance {

class ClipboardSuite {
 public:
  static constexpr std::string_view probe_text = "arisa-conformance";

  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_round_trip(provider::ClipboardProvider& clipboard,
                               Report& report);
  static void check_absent(provider::ClipboardProvider& clipboard,
                           Report& report);
};

}
