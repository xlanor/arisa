#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

namespace arisa::conformance {

class SurfaceSuite {
 public:
  static constexpr int poll_limit = 1000;

  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_size(const provider::SurfaceProvider& surface,
                         Report& report);
  static void check_scale(const provider::SurfaceProvider& surface,
                          Report& report);
  static void check_poll_drains(provider::SurfaceProvider& surface,
                                Report& report);
  static void check_absent(provider::SurfaceProvider& surface, Report& report);
};

}
