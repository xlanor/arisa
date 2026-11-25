#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

#include <string_view>

namespace arisa::conformance {

class AssetsSuite {
 public:
  static constexpr std::string_view probe_path = "arisa-conformance/probe.txt";
  static constexpr std::string_view probe_text = "arisa";

  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_probe(const provider::AssetProvider& assets,
                          Report& report);
  static void check_missing(const provider::AssetProvider& assets,
                            Report& report);
  static void check_path_rules(Report& report);
};

}
