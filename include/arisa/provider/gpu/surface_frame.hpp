#pragma once

#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/render_pass.hpp>

namespace arisa::provider {

struct SurfaceFrame {
  SurfaceId surface{0};
  SurfaceMode mode{SurfaceMode::texture};
  FrameInfo frame{};
  PassRegion viewport{};
  ClipRect scissor{};

  friend constexpr auto operator==(const SurfaceFrame&, const SurfaceFrame&)
      -> bool = default;
};

}
