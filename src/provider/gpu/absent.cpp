#include <arisa/provider/gpu/absent.hpp>

namespace arisa::provider {

auto AbsentGpuProvider::create_texture(TextureDescription)
    -> std::expected<Texture, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentGpuProvider::update_texture(Texture&, TextureRegion, PixelRows)
    -> std::expected<void, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentGpuProvider::begin_frame()
    -> std::expected<GpuFrame, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentGpuProvider::capture() -> std::expected<Image, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

}
