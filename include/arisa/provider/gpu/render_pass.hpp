#pragma once

#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/gpu/texture.hpp>

#include <optional>

namespace arisa::provider {

struct PassRegion {
  float x{0.0F};
  float y{0.0F};
  float width{0.0F};
  float height{0.0F};

  friend constexpr auto operator==(const PassRegion&, const PassRegion&)
      -> bool = default;
};

struct RenderPass {
  TextureId target{0};
  PassRegion region{};
  std::optional<Colour> clear{};

  friend constexpr auto operator==(const RenderPass&, const RenderPass&)
      -> bool = default;
};

}
