#include <arisa/conformance/assets_suite.hpp>
#include <arisa/conformance/audio_suite.hpp>
#include <arisa/conformance/browser_suite.hpp>
#include <arisa/conformance/clipboard_suite.hpp>
#include <arisa/conformance/clock_suite.hpp>
#include <arisa/conformance/conformance_suite.hpp>
#include <arisa/conformance/diagnostics_suite.hpp>
#include <arisa/conformance/fonts_suite.hpp>
#include <arisa/conformance/gpu_suite.hpp>
#include <arisa/conformance/haptics_suite.hpp>
#include <arisa/conformance/input_suite.hpp>
#include <arisa/conformance/lifecycle_suite.hpp>
#include <arisa/conformance/surface_suite.hpp>
#include <arisa/conformance/system_info_suite.hpp>
#include <arisa/conformance/text_input_suite.hpp>
#include <arisa/conformance/threading_suite.hpp>

namespace arisa::conformance {

auto ConformanceSuite::run(provider::ProviderFactory& factory,
                           const ConformanceOptions& options) -> Report {
  Report report{options.trace};
  report.trace("suite factory");
  check_identity(factory, report);
  report.trace("suite threading");
  ThreadingSuite::run(factory, report);
  report.trace("suite clock");
  ClockSuite::run(factory, report);
  report.trace("suite diagnostics");
  DiagnosticsSuite::run(factory, report);
  report.trace("suite assets");
  AssetsSuite::run(factory, report);
  report.trace("suite lifecycle");
  LifecycleSuite::run(factory, report);
  report.trace("suite system_info");
  SystemInfoSuite::run(factory, report);
  report.trace("suite haptics");
  HapticsSuite::run(factory, report);
  report.trace("suite audio");
  AudioSuite::run(factory, report);
  report.trace("suite clipboard");
  ClipboardSuite::run(factory, report);
  report.trace("suite browser");
  BrowserSuite::run(factory, options, report);
  report.trace("suite fonts");
  FontsSuite::run(factory, report);
  report.trace("suite surface");
  SurfaceSuite::run(factory, report);
  report.trace("suite input");
  InputSuite::run(factory, report);
  report.trace("suite gpu");
  GpuSuite::run(factory, report);
  report.trace("suite text_input");
  TextInputSuite::run(factory, options, report);
  report.trace("suites done");
  return report;
}

void ConformanceSuite::check_identity(provider::ProviderFactory& factory,
                                      Report& report) {
  const provider::ProviderInfo info = factory.info();
  report.record("factory.identity",
                !info.name.empty() && info.abi_version == provider::abi_version,
                "info() has an empty name or a mismatched ABI version");
  report.record("factory.required_capabilities",
                factory.supplies(provider::Capability::required),
                "a required capability is not supplied");
}

}
