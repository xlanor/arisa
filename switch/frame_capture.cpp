#include <frame_capture.hpp>

#include <arisa/provider/gpu/image.hpp>

#include <stb_image_write.h>
#include <switch.h>

#include <chrono>
#include <filesystem>
#include <format>

namespace arisa::nx {

FrameCapture::FrameCapture(const NxlinkLog& log) noexcept : log_(log) {}

auto FrameCapture::requested(std::uint64_t held, std::uint64_t pressed) noexcept
    -> bool {
  constexpr std::uint64_t chord = HidNpadButton_ZL | HidNpadButton_ZR;
  return (held & chord) == chord && (pressed & HidNpadButton_Minus) != 0;
}

auto FrameCapture::poll(std::uint64_t held, std::uint64_t pressed,
                        provider::GpuProvider& gpu)
    -> std::optional<std::string> {
  if (!requested(held, pressed)) {
    return std::nullopt;
  }
  std::string outcome = save(gpu);
  log_.get().emit(outcome);
  return outcome;
}

auto FrameCapture::save(provider::GpuProvider& gpu) const -> std::string {
  const auto image = gpu.capture();
  if (!image.has_value()) {
    return std::format("capture failed: {}", image.error().description());
  }
  std::error_code ignored;
  std::filesystem::create_directories(std::string{directory}, ignored);
  const auto stamp = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
  const std::string path = std::format("{}/{}.png", directory, stamp);
  const auto width = static_cast<int>(image->width());
  const auto height = static_cast<int>(image->height());
  const auto stride = width * static_cast<int>(provider::Image::channels);
  if (stbi_write_png(path.c_str(), width, height,
                     static_cast<int>(provider::Image::channels),
                     image->pixels().data(), stride) == 0) {
    return std::format("capture could not be written to {}", path);
  }
  return std::format("Captured {}x{} to {}", width, height, path);
}

}
