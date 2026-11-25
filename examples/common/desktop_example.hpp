#pragma once

#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/provider/sdl3/sdl3_factory.hpp>
#include <example_options.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>

namespace arisa::examples {

class DesktopExample {
 public:
  static constexpr provider::SurfaceSize window_size{
      .width = provider::SurfaceWidth{1280},
      .height = provider::SurfaceHeight{720}};

  [[nodiscard]] static auto factory(const ExampleOptions& options,
                                    std::string title,
                                    provider::Providers overrides = {})
      -> std::expected<provider::sdl3::Sdl3ProviderFactory, std::string>;

  static constexpr float lcd_black_lift = 0.07F;
  static constexpr float lcd_white_drop = 0.05F;
  static constexpr float lcd_shoulder = 1.45F;

  [[nodiscard]] static auto capture(
      provider::GpuProvider& gpu, const std::string& path,
      const std::optional<std::string>& lcd_path = std::nullopt)
      -> std::expected<void, std::string>;

 private:
  static auto washed(std::uint8_t channel) noexcept -> std::uint8_t;

  [[nodiscard]] static auto write(const std::string& path, int width,
                                  int height, std::span<const std::byte> pixels)
      -> std::expected<void, std::string>;
};

}
