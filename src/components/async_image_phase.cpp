#include <arisa/components/async_image_phase.hpp>

#include <utility>

namespace arisa::components {

auto AsyncImagePhase::success(std::shared_ptr<const provider::Image> pixels,
                              float scale) -> AsyncImagePhase {
  AsyncImagePhase phase;
  if (pixels == nullptr) {
    phase.state_ =
        provider::ProviderError{provider::ProviderErrorCode::not_found};
    return phase;
  }
  phase.state_ = Loaded{.pixels = std::move(pixels), .scale = scale};
  return phase;
}

auto AsyncImagePhase::failure(provider::ProviderError error)
    -> AsyncImagePhase {
  AsyncImagePhase phase;
  phase.state_ = error;
  return phase;
}

auto AsyncImagePhase::kind() const noexcept -> Kind {
  if (std::holds_alternative<Loaded>(state_)) {
    return Kind::success;
  }
  if (std::holds_alternative<provider::ProviderError>(state_)) {
    return Kind::failure;
  }
  return Kind::empty;
}

auto AsyncImagePhase::image() const -> std::optional<Image> {
  const Loaded* loaded = std::get_if<Loaded>(&state_);
  if (loaded == nullptr) {
    return std::nullopt;
  }
  return Image{loaded->pixels, loaded->scale};
}

auto AsyncImagePhase::pixels() const noexcept
    -> std::shared_ptr<const provider::Image> {
  const Loaded* loaded = std::get_if<Loaded>(&state_);
  return loaded == nullptr ? nullptr : loaded->pixels;
}

auto AsyncImagePhase::error() const noexcept
    -> std::optional<provider::ProviderError> {
  const auto* failed = std::get_if<provider::ProviderError>(&state_);
  if (failed == nullptr) {
    return std::nullopt;
  }
  return *failed;
}

}
