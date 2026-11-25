#pragma once

#include <arisa/provider/gpu/provider.hpp>

namespace arisa::provider {

class AbsentGpuProvider final : public GpuProvider {
 public:
  [[nodiscard]] auto create_texture(TextureDescription description)
      -> std::expected<Texture, ProviderError> override;
  [[nodiscard]] auto update_texture(Texture& texture, TextureRegion region,
                                    PixelRows pixels)
      -> std::expected<void, ProviderError> override;
  [[nodiscard]] auto begin_frame()
      -> std::expected<GpuFrame, ProviderError> override;
  [[nodiscard]] auto capture() -> std::expected<Image, ProviderError> override;
};

}
