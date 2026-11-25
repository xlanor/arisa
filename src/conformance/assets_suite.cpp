#include <arisa/conformance/assets_suite.hpp>

#include <algorithm>
#include <array>
#include <span>

namespace arisa::conformance {

using provider::AssetPath;
using provider::ProviderErrorCode;

void AssetsSuite::run(provider::ProviderFactory& factory, Report& report) {
  const provider::AssetProvider& assets = factory.assets();
  check_probe(assets, report);
  check_missing(assets, report);
  check_path_rules(report);
}

void AssetsSuite::check_probe(const provider::AssetProvider& assets,
                              Report& report) {
  const auto path = AssetPath::create(probe_path);
  if (!path.has_value()) {
    report.record("assets.probe", false, "the probe path was rejected");
    return;
  }
  const auto blob = assets.read(*path);
  const auto expected = std::as_bytes(std::span{probe_text});
  report.record("assets.probe",
                assets.exists(*path) && blob.has_value() &&
                    std::ranges::equal(blob->bytes(), expected),
                "arisa-conformance/probe.txt did not read back as \"arisa\"");
}

void AssetsSuite::check_missing(const provider::AssetProvider& assets,
                                Report& report) {
  const auto path = AssetPath::create("arisa-conformance/missing.txt");
  if (!path.has_value()) {
    report.record("assets.missing", false, "the missing path was rejected");
    return;
  }
  const auto blob = assets.read(*path);
  report.record("assets.missing",
                !assets.exists(*path) && !blob.has_value() &&
                    blob.error().code() == ProviderErrorCode::not_found,
                "a missing asset was not reported as not_found");
}

void AssetsSuite::check_path_rules(Report& report) {
  static constexpr std::array rejected{
      std::string_view{""},       std::string_view{"/etc/passwd"},
      std::string_view{"a/../b"}, std::string_view{".."},
      std::string_view{"a\\b"},   std::string_view{"a//b"},
      std::string_view{"a/"},     std::string_view{"./a"},
  };
  const bool all_rejected =
      std::ranges::none_of(rejected, [](std::string_view candidate) {
        return AssetPath::create(candidate).has_value();
      });
  report.record("assets.path_rules",
                all_rejected && AssetPath::create("fonts/ui.ttf").has_value(),
                "AssetPath accepted an unsafe path or rejected a safe one");
}

}
