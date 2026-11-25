#include <desktop_example.hpp>

#include <SDL3/SDL_hints.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <print>
#include <utility>
#include <vector>

namespace arisa::examples {

auto DesktopExample::factory(const ExampleOptions& options, std::string title,
                             provider::Providers overrides)
    -> std::expected<provider::sdl3::Sdl3ProviderFactory, std::string> {
  if (options.offscreen().has_value()) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
  }
#if defined(ARISA_VULKAN_LOADER)
  SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, ARISA_VULKAN_LOADER);
#endif
  provider::sdl3::SurfaceMode mode = provider::sdl3::WindowMode{
      .title = std::move(title), .size = window_size};
  if (options.offscreen().has_value()) {
    mode = provider::sdl3::OffscreenMode{.size = *options.offscreen()};
  }
  auto created =
      provider::sdl3::Sdl3ProviderFactory::create(overrides, std::move(mode));
  if (!created.has_value()) {
    return std::unexpected(
        std::format("factory failed: {}", created.error().description()));
  }
  return std::move(*created);
}

auto DesktopExample::washed(std::uint8_t channel) noexcept -> std::uint8_t {
  const float value = static_cast<float>(channel) / 255.0F;
  const float compressed = 1.0F - std::pow(1.0F - value, lcd_shoulder);
  const float shown =
      lcd_black_lift + ((1.0F - lcd_black_lift - lcd_white_drop) * compressed);
  return static_cast<std::uint8_t>(
      std::lround(std::clamp(shown, 0.0F, 1.0F) * 255.0F));
}

auto DesktopExample::write(const std::string& path, int width, int height,
                           std::span<const std::byte> pixels)
    -> std::expected<void, std::string> {
  if (stbi_write_png(path.c_str(), width, height,
                     static_cast<int>(provider::Image::channels), pixels.data(),
                     width * static_cast<int>(provider::Image::channels)) ==
      0) {
    return std::unexpected(std::format("could not write {}", path));
  }
  std::println("captured {}x{} to {}", width, height, path);
  return {};
}

auto DesktopExample::capture(provider::GpuProvider& gpu,
                             const std::string& path,
                             const std::optional<std::string>& lcd_path)
    -> std::expected<void, std::string> {
  const auto image = gpu.capture();
  if (!image.has_value()) {
    return std::unexpected(
        std::format("capture failed: {}", image.error().description()));
  }
  const int width = static_cast<int>(image->width());
  const int height = static_cast<int>(image->height());
  if (auto written = write(path, width, height, image->pixels());
      !written.has_value() || !lcd_path.has_value()) {
    return written;
  }
  std::vector<std::byte> washout(image->pixels().begin(),
                                 image->pixels().end());
  for (std::size_t index = 0; index < washout.size(); ++index) {
    if (index % provider::Image::channels == provider::Image::channels - 1) {
      continue;
    }
    washout[index] =
        std::byte{washed(std::to_integer<std::uint8_t>(washout[index]))};
  }
  return write(*lcd_path, width, height, washout);
}

}
