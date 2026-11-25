#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

#include <array>
#include <cstdint>

namespace arisa::conformance {

class GpuSuite {
 public:
  static constexpr provider::Colour clear_colour{
      .red = 0.0F, .green = 0.0F, .blue = 1.0F, .alpha = 1.0F};
  static constexpr std::array<std::uint8_t, 4> quad_rgba{255, 0, 0, 255};
  static constexpr int tolerance = 8;

  static void run(provider::ProviderFactory& factory, Report& report);

  static auto matches(std::array<std::uint8_t, 4> actual,
                      std::array<std::uint8_t, 4> expected) noexcept -> bool;

 private:
  static void check_texture_lifecycle(provider::GpuProvider& gpu,
                                      Report& report);
  static void check_texture_rejects(provider::GpuProvider& gpu, Report& report);
  static void check_frame_discard(provider::GpuProvider& gpu, Report& report);
  static void check_frame_present(provider::GpuProvider& gpu,
                                  provider::SurfaceSize size, Report& report);
  static void check_unwritten_texels(provider::GpuProvider& gpu,
                                     provider::SurfaceSize size,
                                     Report& report);
  static auto present_half_written(provider::GpuProvider& gpu,
                                   provider::SurfaceSize size) -> bool;
  static void check_absent(provider::GpuProvider& gpu, Report& report);
};

}
