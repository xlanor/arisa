#include <arisa/conformance/surface_suite.hpp>

#include <cmath>

namespace arisa::conformance {

void SurfaceSuite::run(provider::ProviderFactory& factory, Report& report) {
  provider::SurfaceProvider& surface = factory.surface();
  if (!factory.supplies(provider::Capability::surface)) {
    check_absent(surface, report);
    return;
  }
  check_size(surface, report);
  check_scale(surface, report);
  check_poll_drains(surface, report);
}

void SurfaceSuite::check_size(const provider::SurfaceProvider& surface,
                              Report& report) {
  report.record("surface.size", !surface.size().empty(),
                "size() reported a zero width or height");
}

void SurfaceSuite::check_scale(const provider::SurfaceProvider& surface,
                               Report& report) {
  const float scale = surface.scale().value();
  report.record("surface.scale", std::isfinite(scale) && scale > 0.0F,
                "scale() was not a positive finite number");
}

void SurfaceSuite::check_poll_drains(provider::SurfaceProvider& surface,
                                     Report& report) {
  bool drained = false;
  for (int call = 0; call < poll_limit && !drained; ++call) {
    drained = !surface.poll().has_value();
  }
  report.record("surface.poll_drains", drained,
                "poll() kept returning events for 1000 calls");
}

void SurfaceSuite::check_absent(provider::SurfaceProvider& surface,
                                Report& report) {
  report.record("surface.absent",
                surface.size().empty() &&
                    surface.scale() == provider::SurfaceScale::unit() &&
                    !surface.poll().has_value(),
                "an unsupplied surface provider did not behave as absent");
}

}
