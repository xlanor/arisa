#pragma once

#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/render_pass.hpp>

namespace arisa::provider {

struct SurfaceUnderlay {
  SurfaceId surface{0};
  PassRegion viewport{};
  ClipRect scissor{};

  friend constexpr auto operator==(const SurfaceUnderlay&,
                                   const SurfaceUnderlay&) -> bool = default;
};

}
