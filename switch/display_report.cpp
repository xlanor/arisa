#include <display_report.hpp>

#include <format>

namespace arisa::nx {

DisplayReport::DisplayReport(const NxlinkLog& log) noexcept : log_(log) {}

auto DisplayReport::read(const provider::SurfaceProvider& surface,
                         NWindow* window, composition::Rect root) noexcept
    -> Reading {
  Reading reading{.mode = appletGetOperationMode(),
                  .framebuffer = surface.size(),
                  .scale = surface.scale().value(),
                  .window_width = 0,
                  .window_height = 0,
                  .crop = window != nullptr ? window->crop : BqRect{},
                  .root_width = root.width,
                  .root_height = root.height};
  if (window != nullptr) {
    static_cast<void>(nwindowGetDimensions(window, &reading.window_width,
                                           &reading.window_height));
  }
  return reading;
}

void DisplayReport::observe(const provider::SurfaceProvider& surface,
                            NWindow* window, composition::Rect root) {
  const Reading reading = read(surface, window, root);
  if (last_.has_value() && *last_ == reading) {
    return;
  }
  last_ = reading;
  log_.get().emit(std::format(
      "display: mode {} | framebuffer {}x{} | window {}x{} | crop {},{} - "
      "{},{} | layout root {:.1f}x{:.1f} pt | scale {:.2f}",
      reading.mode == AppletOperationMode_Console ? "docked" : "handheld",
      reading.framebuffer.width.value(), reading.framebuffer.height.value(),
      reading.window_width, reading.window_height, reading.crop.left,
      reading.crop.top, reading.crop.right, reading.crop.bottom,
      reading.root_width, reading.root_height, reading.scale));
}

}
