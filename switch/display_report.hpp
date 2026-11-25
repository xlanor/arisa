#pragma once

#include <arisa/composition/rect.hpp>
#include <arisa/provider/surface/provider.hpp>
#include <nxlink_log.hpp>

#include <switch.h>

#include <functional>
#include <optional>

namespace arisa::nx {

class DisplayReport {
 public:
  explicit DisplayReport(const NxlinkLog& log) noexcept;

  void observe(const provider::SurfaceProvider& surface, NWindow* window,
               composition::Rect root);

 private:
  struct Reading {
    AppletOperationMode mode{AppletOperationMode_Handheld};
    provider::SurfaceSize framebuffer{};
    float scale{1.0F};
    u32 window_width{0};
    u32 window_height{0};
    BqRect crop{};
    float root_width{0.0F};
    float root_height{0.0F};

    friend auto operator==(const Reading& left, const Reading& right) noexcept
        -> bool {
      return left.mode == right.mode && left.framebuffer == right.framebuffer &&
             left.scale == right.scale &&
             left.window_width == right.window_width &&
             left.window_height == right.window_height &&
             left.crop.left == right.crop.left &&
             left.crop.top == right.crop.top &&
             left.crop.right == right.crop.right &&
             left.crop.bottom == right.crop.bottom &&
             left.root_width == right.root_width &&
             left.root_height == right.root_height;
    }
  };

  static auto read(const provider::SurfaceProvider& surface, NWindow* window,
                   composition::Rect root) noexcept -> Reading;

  std::reference_wrapper<const NxlinkLog> log_;
  std::optional<Reading> last_;
};

}
